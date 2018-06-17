/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include "prim.h"
#include "infer.h"
#include <llvm-c/Core.h>
#include "llvm-c/Analysis.h"
#include "codegen/codegen.h"
#include "symtable.h"
#include "runtime/runtime.h"

//=====================================================
// Forward Declarations
//=====================================================
void necro_init_prim_defs(NecroPrimTypes* prim_types, NecroIntern* intern);

//=====================================================
// Symbols
//=====================================================
NecroPrimTypes necro_create_prim_types(NecroIntern* intern)
{
    // PrimSymbols
    return (NecroPrimTypes)
    {
        .arena    = necro_create_paged_arena(),
        .defs     = NULL,
        .def_head = NULL,
        .llvm_mod = NULL
    };
}

//=====================================================
// NecroPrimDefs
//=====================================================
NecroPrimDef* necro_create_prim_def(NecroPrimTypes* prim_types, NECRO_PRIM_DEF prim_def_type, NecroCon* global_name)
{
    NecroPrimDef* def = necro_paged_arena_alloc(&prim_types->arena, sizeof(NecroPrimDef));
    def->next         = NULL;
    def->type         = prim_def_type;
    def->global_name  = global_name;
    if (prim_types->defs == NULL)
    {
        prim_types->def_head = def;
        prim_types->defs     = def;
    }
    else
    {
        prim_types->defs->next = def;
        prim_types->defs       = def;
    }
    return def;
}

NecroPrimDef* necro_prim_def_data(NecroPrimTypes* prim_types, NecroIntern* intern, NecroCon* global_name, NecroASTNode* data_declaration_ast)
{
    assert(prim_types != NULL);
    assert(intern != NULL);
    assert(data_declaration_ast != NULL);
    assert(data_declaration_ast->type == NECRO_AST_DATA_DECLARATION);
    assert(data_declaration_ast->data_declaration.simpletype != NULL);
    assert(data_declaration_ast->data_declaration.simpletype->type == NECRO_AST_SIMPLE_TYPE);
    assert(data_declaration_ast->data_declaration.simpletype->simple_type.type_con != NULL);
    assert(data_declaration_ast->data_declaration.simpletype->simple_type.type_con->type == NECRO_AST_CONID);
    NecroSymbol type_symbol = data_declaration_ast->data_declaration.simpletype->simple_type.type_con->conid.symbol;
    NecroPrimDef*   data_def  = necro_create_prim_def(prim_types, NECRO_PRIM_DEF_DATA, global_name);
    NecroCon        type_name = (NecroCon) { .id = { 0 }, .symbol = type_symbol };
    if (global_name != NULL)
        *global_name = type_name;
    data_def->name                             = type_name;
    data_def->global_name                      = global_name;
    data_def->data_def.type_type               = NULL;
    data_def->data_def.type_fully_applied_type = NULL;
    data_def->data_def.data_declaration_ast    = data_declaration_ast;
    return data_def;
}

NecroPrimDef* necro_prim_def_class(NecroPrimTypes* prim_types, NecroIntern* intern, NecroCon* global_name, NecroASTNode* type_class_ast)
{
    assert(prim_types != NULL);
    assert(intern != NULL);
    assert(type_class_ast != NULL);
    assert(type_class_ast->type == NECRO_AST_TYPE_CLASS_DECLARATION);
    assert(type_class_ast->type_class_declaration.tycls != NULL);
    assert(type_class_ast->type_class_declaration.tycls->type == NECRO_AST_CONID);
    assert(type_class_ast->type_class_declaration.tyvar != NULL);
    assert(type_class_ast->type_class_declaration.tyvar->type == NECRO_AST_VARIABLE);
    NecroSymbol     class_symbol = type_class_ast->type_class_declaration.tycls->conid.symbol;
    NecroPrimDef*   class_def    = necro_create_prim_def(prim_types, NECRO_PRIM_DEF_CLASS, global_name);
    NecroCon        class_name   = (NecroCon) { .id = { 0 }, .symbol = class_symbol };
    if (global_name != NULL)
        *global_name = class_name;
    class_def->name                     = class_name;
    class_def->global_name              = global_name;
    class_def->class_def.type_class     = NULL;
    class_def->class_def.type_class_ast = type_class_ast;
    return class_def;
}

NecroPrimDef* necro_prim_def_instance(NecroPrimTypes* prim_types, NecroIntern* intern, NecroASTNode* instance_ast)
{
    assert(prim_types != NULL);
    assert(intern != NULL);
    assert(instance_ast != NULL);
    assert(instance_ast->type == NECRO_AST_TYPE_CLASS_INSTANCE);
    assert(instance_ast->type_class_instance.qtycls != NULL);
    assert(instance_ast->type_class_instance.qtycls->type == NECRO_AST_CONID);
    assert(instance_ast->type_class_instance.inst != NULL);
    assert(instance_ast->type_class_instance.inst->type == NECRO_AST_CONID || instance_ast->type_class_instance.inst->type == NECRO_AST_CONSTRUCTOR);
    NecroPrimDef* instance_def = necro_create_prim_def(prim_types, NECRO_PRIM_DEF_INSTANCE, NULL);
    instance_def->global_name               = NULL;
    instance_def->instance_def.instance     = NULL;
    instance_def->instance_def.instance_ast = instance_ast;
    return instance_def;
}

NecroPrimDef* necro_prim_def_fun(NecroPrimTypes* prim_types, NecroIntern* intern, NecroCon* global_name, NecroASTNode* type_sig_ast)
{
    assert(type_sig_ast->type = NECRO_AST_TYPE_SIGNATURE);
    NecroSymbol     fun_symbol      = type_sig_ast->type_signature.var->variable.symbol;
    NecroPrimDef*   fun_def         = necro_create_prim_def(prim_types, NECRO_PRIM_DEF_FUN, global_name);
    NecroCon        fun_name        = (NecroCon) { .id = { 0 }, .symbol = fun_symbol };
    if (global_name != NULL)
        *global_name = fun_name;
    fun_def->name                 = fun_name;
    fun_def->fun_def.type         = NULL;
    fun_def->fun_def.type_sig_ast = type_sig_ast;
    return fun_def;
}

NecroPrimDef* necro_prim_def_bin_op(NecroPrimTypes* prim_types, NecroIntern* intern, NecroCon* global_name, NecroASTNode* type_sig_ast, NecroASTNode* definition_ast)
{
    assert(type_sig_ast != NULL);
    assert(type_sig_ast->type = NECRO_AST_TYPE_SIGNATURE);
    assert(definition_ast != NULL);
    NecroSymbol   bin_op_symbol = type_sig_ast->type_signature.var->variable.symbol;
    NecroPrimDef* bin_op_def    = necro_create_prim_def(prim_types, NECRO_PRIM_DEF_BIN_OP, global_name);
    NecroCon      bin_op_name   = (NecroCon) { .id = { 0 }, .symbol = bin_op_symbol };
    if (global_name != NULL)
        *global_name = bin_op_name;
    bin_op_def->name                      = bin_op_name;
    bin_op_def->bin_op_def.type           = NULL;
    bin_op_def->bin_op_def.type_sig_ast   = type_sig_ast;
    bin_op_def->bin_op_def.definition_ast = definition_ast;
    return bin_op_def;
}

NECRO_RETURN_CODE necro_prim_build_scope(NecroPrimTypes* prim_types, NecroScopedSymTable* scoped_symtable)
{
    NecroPrimDef* def = prim_types->def_head;
    while (def != NULL)
    {
        switch (def->type)
        {
        case NECRO_PRIM_DEF_DATA:
        {
            necro_build_scopes_go(scoped_symtable, def->data_def.data_declaration_ast);
            break;
        }
        case NECRO_PRIM_DEF_CLASS:
        {
            necro_build_scopes_go(scoped_symtable, def->class_def.type_class_ast);
            break;
        }
        case NECRO_PRIM_DEF_INSTANCE:
        {
            necro_build_scopes_go(scoped_symtable, def->instance_def.instance_ast);
            break;
        }
        case NECRO_PRIM_DEF_FUN:
        {
            necro_build_scopes_go(scoped_symtable, def->fun_def.type_sig_ast);
            break;
        }
        case NECRO_PRIM_DEF_BIN_OP:
        {
            necro_build_scopes_go(scoped_symtable, def->bin_op_def.type_sig_ast);
            necro_build_scopes_go(scoped_symtable, def->bin_op_def.definition_ast);
            break;
        }
        default: assert(false); break;
        }
        if (scoped_symtable->error.return_code != NECRO_SUCCESS)
            return scoped_symtable->error.return_code;
        def = def->next;
    }
    return scoped_symtable->error.return_code;
}

NECRO_RETURN_CODE necro_prim_rename(NecroPrimTypes* prim_types, NecroRenamer* renamer)
{
    renamer->arena = &prim_types->arena;
    renamer->arena = NULL;
    NecroPrimDef* def = prim_types->def_head;
    while (def != NULL)
    {
        switch (def->type)
        {
        case NECRO_PRIM_DEF_DATA:
        {
            necro_rename_declare_pass(renamer, &prim_types->arena, def->data_def.data_declaration_ast);
            necro_rename_var_pass(renamer, &prim_types->arena, def->data_def.data_declaration_ast);
            if (def->global_name != NULL)
            {
                def->global_name->symbol = def->data_def.data_declaration_ast->data_declaration.simpletype->simple_type.type_con->conid.symbol;
                def->global_name->id     = def->data_def.data_declaration_ast->data_declaration.simpletype->simple_type.type_con->conid.id;
            }
            break;
        }
        case NECRO_PRIM_DEF_CLASS:
        {
            necro_rename_declare_pass(renamer, &prim_types->arena, def->class_def.type_class_ast);
            necro_rename_var_pass(renamer, &prim_types->arena, def->class_def.type_class_ast);
            if (def->global_name != NULL)
            {
                def->global_name->symbol = def->class_def.type_class_ast->type_class_declaration.tycls->conid.symbol;
                def->global_name->id     = def->class_def.type_class_ast->type_class_declaration.tycls->conid.id;
            }
            break;
        }
        case NECRO_PRIM_DEF_FUN:
        {
            necro_rename_declare_pass(renamer, &prim_types->arena, def->fun_def.type_sig_ast);
            necro_rename_var_pass(renamer, &prim_types->arena, def->fun_def.type_sig_ast);
            if (def->global_name != NULL)
            {
                def->global_name->symbol = def->fun_def.type_sig_ast->type_signature.var->variable.symbol;
                def->global_name->id     = def->fun_def.type_sig_ast->type_signature.var->variable.id;
            }
            break;
        }
        case NECRO_PRIM_DEF_BIN_OP:
        {
            necro_rename_declare_pass(renamer, &prim_types->arena, def->bin_op_def.type_sig_ast);
            necro_rename_declare_pass(renamer, &prim_types->arena, def->bin_op_def.definition_ast);
            necro_rename_var_pass(renamer, &prim_types->arena, def->bin_op_def.type_sig_ast);
            necro_rename_var_pass(renamer, &prim_types->arena, def->bin_op_def.definition_ast);
            if (def->global_name != NULL)
            {
                def->global_name->symbol = def->bin_op_def.type_sig_ast->type_signature.var->variable.symbol;
                def->global_name->id     = def->bin_op_def.type_sig_ast->type_signature.var->variable.id;
            }
            break;
        }
        case NECRO_PRIM_DEF_INSTANCE:
        {
            necro_rename_declare_pass(renamer, &prim_types->arena, def->instance_def.instance_ast);
            necro_rename_var_pass(renamer, &prim_types->arena, def->instance_def.instance_ast);
            break;
        }
        default: assert(false); break;
        }

        if (renamer->error.return_code != NECRO_SUCCESS)
            return renamer->error.return_code;
        def = def->next;
    }
    return renamer->error.return_code;
}

NECRO_RETURN_CODE necro_prim_infer(NecroPrimTypes* prim_types, NecroDependencyAnalyzer* d_analyzer, NecroInfer* infer, NECRO_PHASE phase)
{
    NecroPrimDef* def  = prim_types->def_head;
    NecroASTNode* top  = NULL;
    NecroASTNode* head = NULL;
    // Turn into single top_level declaration
    while (def != NULL)
    {
        switch (def->type)
        {
        case NECRO_PRIM_DEF_DATA:
            if (top == NULL)
            {
                top  = necro_create_top_level_declaration_list(&prim_types->arena, def->data_def.data_declaration_ast, NULL);
                head = top;
            }
            else
            {
                top->top_declaration.next_top_decl = necro_create_top_level_declaration_list(&prim_types->arena, def->data_def.data_declaration_ast, NULL);
                top                                = top->top_declaration.next_top_decl;
            }
            break;
        case NECRO_PRIM_DEF_CLASS:
            if (top == NULL)
            {
                top  = necro_create_top_level_declaration_list(&prim_types->arena, def->class_def.type_class_ast, NULL);
                head = top;
            }
            else
            {
                top->top_declaration.next_top_decl = necro_create_top_level_declaration_list(&prim_types->arena, def->class_def.type_class_ast, NULL);
                top                                = top->top_declaration.next_top_decl;
            }
            break;
        case NECRO_PRIM_DEF_INSTANCE:
            if (top == NULL)
            {
                top  = necro_create_top_level_declaration_list(&prim_types->arena, def->instance_def.instance_ast, NULL);
                head = top;
            }
            else
            {
                top->top_declaration.next_top_decl = necro_create_top_level_declaration_list(&prim_types->arena, def->instance_def.instance_ast, NULL);
                top                                = top->top_declaration.next_top_decl;
            }
            break;
        case NECRO_PRIM_DEF_FUN:
            if (top == NULL)
            {
                top  = necro_create_top_level_declaration_list(&prim_types->arena, def->fun_def.type_sig_ast, NULL);
                head = top;
            }
            else
            {
                top->top_declaration.next_top_decl = necro_create_top_level_declaration_list(&prim_types->arena, def->fun_def.type_sig_ast, NULL);
                top                                = top->top_declaration.next_top_decl;
            }
            break;
        case NECRO_PRIM_DEF_BIN_OP:
            if (top == NULL)
            {
                top  = necro_create_top_level_declaration_list(&prim_types->arena, def->bin_op_def.type_sig_ast, necro_create_top_level_declaration_list(&prim_types->arena, def->bin_op_def.definition_ast, NULL));
                head = top;
            }
            else
            {
                top->top_declaration.next_top_decl = necro_create_top_level_declaration_list(&prim_types->arena, def->bin_op_def.type_sig_ast, necro_create_top_level_declaration_list(&prim_types->arena, def->bin_op_def.definition_ast, NULL));
                top                                = top->top_declaration.next_top_decl->top_declaration.next_top_decl;
            }
            break;
        default: assert(false); break;
        }
        def = def->next;
    }
    // Run dependency analyser
    necro_dependency_analyze_ast(d_analyzer, &prim_types->arena, head);
    // Run type checking on the whole top level declarations
    if (phase != NECRO_PHASE_DEPENDENCY_ANALYSIS)
        necro_infer_go(infer, head);

    if (necro_is_infer_error(infer))
    {
        necro_print_reified_ast_node(head, infer->intern);
    }

    return infer->error.return_code;
}

// TODO: Finish
void necro_derive_eq(NecroPrimTypes* prim_types, NecroIntern* intern, NecroPrimDef* dat_def)
{
}

// TODO: Save % for rational types
void necro_create_prim_num(NecroPrimTypes* prim_types, NecroIntern* intern, NecroCon* global_name, const char* data_type_name)
{
    NecroASTNode* s_type   = necro_create_simple_type_ast(&prim_types->arena, intern, data_type_name, NULL);
    NecroPrimDef* data_def = necro_prim_def_data(prim_types, intern, global_name, necro_create_data_declaration_ast(&prim_types->arena, intern, s_type, NULL));
}

NecroASTNode* necro_make_poly_con1(NecroPagedArena* arena, NecroIntern* intern, const char* data_type_name)
{
    NecroASTNode* type_var_ast = necro_create_variable_ast(arena, intern, "a", NECRO_VAR_TYPE_VAR_DECLARATION);
    NecroASTNode* type_constr  = necro_create_data_constructor_ast(arena, intern, data_type_name, necro_create_ast_list(arena, type_var_ast, NULL));
    type_constr->constructor.conid->conid.con_type = NECRO_CON_TYPE_VAR;
    // type_constr->conid.con_type = NECRO_CON_TYPE_VAR;
    return type_constr;
}

NecroASTNode* necro_make_num_context(NecroPagedArena* arena, NecroIntern* intern, const char* class_name, bool is_polymorphic)
{
    if (is_polymorphic)
        return necro_create_context(arena, intern, class_name, "a", NULL);
    else
        return NULL;
}

NecroASTNode* necro_make_num_con(NecroPagedArena* arena, NecroIntern* intern, const char* data_type_name, bool is_polymorphic)
{
    if (is_polymorphic)
        return necro_make_poly_con1(arena, intern, data_type_name);
    else
        return necro_create_conid_ast(arena, intern, data_type_name, NECRO_CON_TYPE_VAR);
}

NecroASTNode* necro_make_bin_math_ast(NecroPrimTypes* prim_types, NecroIntern* intern, const char* data_type_name, bool is_poly)
{
    return necro_create_fun_ast(&prim_types->arena,
        necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly),
        necro_create_fun_ast(&prim_types->arena,
            necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly),
            necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly)));
}

NecroASTNode* necro_make_unary_math_ast(NecroPrimTypes* prim_types, NecroIntern* intern, const char* data_type_name, bool is_poly)
{
    return necro_create_fun_ast(&prim_types->arena,
        necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly),
        necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly));
}

// TODO: Deriving mechanism
void necro_create_prim_num_instances(NecroPrimTypes* prim_types, NecroIntern* intern, const char* data_type_name, bool is_poly, bool is_fractional)
{
    NecroSymbol   data_name  = necro_intern_string(intern, data_type_name);
    NecroASTNode* bool_conid = necro_create_conid_ast(&prim_types->arena, intern, "Bool", NECRO_CON_TYPE_VAR);

    //--------------
    // Num

    const char*   add_symbol        = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primAdd@"), data_name));
    NecroPrimDef* add_type_def      = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, add_symbol, NULL, necro_make_bin_math_ast(prim_types, intern, data_type_name, is_poly), NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    const char*   sub_symbol        = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primSub@"), data_name));
    NecroPrimDef* sub_type_def      = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, sub_symbol, NULL, necro_make_bin_math_ast(prim_types, intern, data_type_name, is_poly), NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    const char*   mul_symbol        = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primMul@"), data_name));
    NecroPrimDef* mul_type_def      = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, mul_symbol, NULL, necro_make_bin_math_ast(prim_types, intern, data_type_name, is_poly), NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    const char*   abs_symbol        = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primAbs@"), data_name));
    NecroPrimDef* abs_type_def      = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, abs_symbol, NULL, necro_make_unary_math_ast(prim_types, intern, data_type_name, is_poly), NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    const char*   signum_symbol     = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primSignum@"), data_name));
    NecroPrimDef* signum_type_def   = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, signum_symbol, NULL, necro_make_unary_math_ast(prim_types, intern, data_type_name, is_poly), NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    const char*   from_int_symbol   = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primFromInt@"), data_name));
    NecroASTNode* from_int_ast      = necro_create_fun_ast(&prim_types->arena, necro_create_conid_ast(&prim_types->arena, intern, "Int", NECRO_CON_TYPE_VAR), necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly));
    NecroPrimDef* from_int_type_def = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, from_int_symbol, NULL, from_int_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    NecroASTNode* num_method_list  =
        necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "add", necro_create_variable_ast(&prim_types->arena, intern, add_symbol, NECRO_VAR_VAR)),
            necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "sub", necro_create_variable_ast(&prim_types->arena, intern, sub_symbol, NECRO_VAR_VAR)),
                necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "mul", necro_create_variable_ast(&prim_types->arena, intern, mul_symbol, NECRO_VAR_VAR)),
                    necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "abs", necro_create_variable_ast(&prim_types->arena, intern, abs_symbol, NECRO_VAR_VAR)),
                        necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "signum", necro_create_variable_ast(&prim_types->arena, intern, signum_symbol, NECRO_VAR_VAR)),
                            necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "fromInt", necro_create_variable_ast(&prim_types->arena, intern, from_int_symbol, NECRO_VAR_VAR)), NULL))))));
    NecroASTNode* num_instance_ast = necro_create_instance_ast(&prim_types->arena, intern, "Num", necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly), necro_make_num_context(&prim_types->arena, intern, "Num", is_poly), num_method_list);
    NecroPrimDef* num_instance_def = necro_prim_def_instance(prim_types, intern, num_instance_ast);

    //--------------
    // Fractional
    if (is_fractional)
    {
        const char*   div_symbol     = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primDiv@"), data_name));
        NecroPrimDef* div_type_def   = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, div_symbol, NULL, necro_make_bin_math_ast(prim_types, intern, data_type_name, is_poly), NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

        const char*   recip_symbol   = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primRecip@"), data_name));
        NecroPrimDef* recip_type_def = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, recip_symbol, NULL, necro_make_unary_math_ast(prim_types, intern, data_type_name, is_poly), NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

        const char*   from_rational_symbol   = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primFromRational@"), data_name));
        NecroASTNode* from_rational_ast      = necro_create_fun_ast(&prim_types->arena, necro_create_conid_ast(&prim_types->arena, intern, "Rational", NECRO_CON_TYPE_VAR), necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly));
        NecroPrimDef* from_rational_type_def = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, from_rational_symbol, NULL, from_rational_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

        NecroASTNode* fractional_method_list =
            necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "div", necro_create_variable_ast(&prim_types->arena, intern, div_symbol, NECRO_VAR_VAR)),
                necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "recip", necro_create_variable_ast(&prim_types->arena, intern, recip_symbol, NECRO_VAR_VAR)),
                    necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "fromRational", necro_create_variable_ast(&prim_types->arena, intern, from_rational_symbol, NECRO_VAR_VAR)), NULL)));
        NecroASTNode* fractional_instance_ast = necro_create_instance_ast(&prim_types->arena, intern, "Fractional", necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly), necro_make_num_context(&prim_types->arena, intern, "Fractional", is_poly), fractional_method_list);
        NecroPrimDef* fractional_instance_def = necro_prim_def_instance(prim_types, intern, fractional_instance_ast);
    }

    //--------------
    // Eq
    NecroASTNode* bin_comp_ast = necro_create_fun_ast(&prim_types->arena, necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly), necro_create_fun_ast(&prim_types->arena, necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly), bool_conid));
    const char*   eq_symbol    = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primEq@"), data_name));
    NecroPrimDef* eq_type_def  = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, eq_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    const char*   neq_symbol   = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primNEq@"), data_name));
    NecroPrimDef* neq_type_def = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, neq_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    NecroASTNode* eq_method_list =
        necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "eq", necro_create_variable_ast(&prim_types->arena, intern, eq_symbol, NECRO_VAR_VAR)),
            necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "neq", necro_create_variable_ast(&prim_types->arena, intern, neq_symbol, NECRO_VAR_VAR)), NULL));

    NecroASTNode* eq_instance_ast = necro_create_instance_ast(&prim_types->arena, intern, "Eq", necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly), necro_make_num_context(&prim_types->arena, intern, "Eq", is_poly), eq_method_list);
    NecroPrimDef* eq_instance_def = necro_prim_def_instance(prim_types, intern, eq_instance_ast);

    //--------------
    // Ord
    const char*   gt_symbol    = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primGT@"), data_name));
    NecroPrimDef* gt_type_def  = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, gt_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    const char*   lt_symbol    = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primLT@"), data_name));
    NecroPrimDef* lt_type_def  = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, lt_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    const char*   gte_symbol   = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primGTE@"), data_name));
    NecroPrimDef* gte_type_def = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, gte_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    const char*   lte_symbol   = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primLTE@"), data_name));
    NecroPrimDef* lte_type_def = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, lte_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    NecroASTNode* ord_method_list =
        necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "lt", necro_create_variable_ast(&prim_types->arena, intern, lt_symbol, NECRO_VAR_VAR)),
            necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "gt", necro_create_variable_ast(&prim_types->arena, intern, gt_symbol, NECRO_VAR_VAR)),
                necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "lte", necro_create_variable_ast(&prim_types->arena, intern, lte_symbol, NECRO_VAR_VAR)),
                    necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "gte", necro_create_variable_ast(&prim_types->arena, intern, gte_symbol, NECRO_VAR_VAR)), NULL))));
    NecroASTNode* ord_instance_ast = necro_create_instance_ast(&prim_types->arena, intern, "Ord", necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly), necro_make_num_context(&prim_types->arena, intern, "Ord", is_poly), ord_method_list);
    NecroPrimDef* ord_instance_def = necro_prim_def_instance(prim_types, intern, ord_instance_ast);
}

NecroASTNode* necro_create_class_comp_sig(NecroPagedArena* arena, NecroIntern* intern, const char* sig_name)
{
    return
        necro_create_fun_type_sig_ast(arena, intern, sig_name, NULL,
            necro_create_fun_ast(arena,
                necro_create_variable_ast(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                necro_create_fun_ast(arena,
                    necro_create_variable_ast(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                    necro_create_conid_ast(arena, intern, "Bool", NECRO_CON_TYPE_VAR))), NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
}

NecroASTNode* necro_create_class_bin_op_sig(NecroPagedArena* arena, NecroIntern* intern, const char* sig_name)
{
    return
        necro_create_fun_type_sig_ast(arena, intern, sig_name, NULL,
            necro_create_fun_ast(arena,
                necro_create_variable_ast(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                necro_create_fun_ast(arena,
                    necro_create_variable_ast(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                    necro_create_variable_ast(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR))), NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
}

NecroASTNode* necro_create_class_unary_op_sig(NecroPagedArena* arena, NecroIntern* intern, const char* sig_name)
{
    return
        necro_create_fun_type_sig_ast(arena, intern, sig_name, NULL,
            necro_create_fun_ast(arena,
                necro_create_variable_ast(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                necro_create_variable_ast(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR)), NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
}

NecroASTNode* necro_create_num_bin_op_sig(NecroPagedArena* arena, NecroIntern* intern, const char* sig_name)
{
    return
        necro_create_fun_type_sig_ast(arena, intern, sig_name, necro_create_context(arena, intern, "Num", "a", NULL),
            necro_create_fun_ast(arena,
                necro_create_variable_ast(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                necro_create_fun_ast(arena,
                    necro_create_variable_ast(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                    necro_create_variable_ast(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR))), NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
}

NecroASTNode* necro_create_fractional_bin_op_sig(NecroPagedArena* arena, NecroIntern* intern, const char* sig_name)
{
    return
        necro_create_fun_type_sig_ast(arena, intern, sig_name, necro_create_context(arena, intern, "Fractional", "a", NULL),
            necro_create_fun_ast(arena,
                necro_create_variable_ast(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                necro_create_fun_ast(arena,
                    necro_create_variable_ast(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                    necro_create_variable_ast(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR))), NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
}

NecroASTNode* necro_create_eq_comp_sig(NecroPagedArena* arena, NecroIntern* intern, const char* sig_name)
{
    return
        necro_create_fun_type_sig_ast(arena, intern, sig_name, necro_create_context(arena, intern, "Eq", "a", NULL),
            necro_create_fun_ast(arena,
                necro_create_variable_ast(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                necro_create_fun_ast(arena,
                    necro_create_variable_ast(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                    necro_create_conid_ast(arena, intern, "Bool", NECRO_CON_TYPE_VAR))), NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
}

NecroASTNode* necro_create_ord_comp_sig(NecroPagedArena* arena, NecroIntern* intern, const char* sig_name)
{
    return
        necro_create_fun_type_sig_ast(arena, intern, sig_name, necro_create_context(arena, intern, "Ord", "a", NULL),
            necro_create_fun_ast(arena,
                necro_create_variable_ast(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                necro_create_fun_ast(arena,
                    necro_create_variable_ast(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                    necro_create_conid_ast(arena, intern, "Bool", NECRO_CON_TYPE_VAR))), NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
}

void necro_bool_instances(NecroPagedArena* arena, NecroIntern* intern)
{
    // //--------------
    // // Eq
    // NecroASTNode* bin_comp_ast = necro_create_fun_ast(&prim_types->arena, type_conid, necro_create_fun_ast(&prim_types->arena, type_conid, bool_conid));
    // const char*   eq_symbol    = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primEq@"), type_conid->conid.symbol));
    // NecroPrimDef* eq_type_def  = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, eq_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    // const char*   neq_symbol   = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primNEq@"), type_conid->conid.symbol));
    // NecroPrimDef* neq_type_def = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, neq_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    // NecroASTNode* eq_method_list =
    //     necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "eq", necro_create_variable_ast(&prim_types->arena, intern, eq_symbol, NECRO_VAR_VAR)),
    //         necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "neq", necro_create_variable_ast(&prim_types->arena, intern, neq_symbol, NECRO_VAR_VAR)), NULL));
    // NecroASTNode* eq_instance_ast = necro_create_instance_ast(&prim_types->arena, intern, "Eq", type_conid, NULL, eq_method_list);
    // NecroPrimDef* eq_instance_def = necro_prim_def_instance(prim_types, intern, eq_instance_ast);

    // //--------------
    // // Ord
    // const char*   gt_symbol    = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primGT@"), type_conid->conid.symbol));
    // NecroPrimDef* gt_type_def  = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, gt_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    // const char*   lt_symbol    = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primLT@"), type_conid->conid.symbol));
    // NecroPrimDef* lt_type_def  = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, lt_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    // const char*   gte_symbol   = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primGTE@"), type_conid->conid.symbol));
    // NecroPrimDef* gte_type_def = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, gte_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    // const char*   lte_symbol   = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primLTE@"), type_conid->conid.symbol));
    // NecroPrimDef* lte_type_def = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, lte_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    // NecroASTNode* ord_method_list =
    //     necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "lt", necro_create_variable_ast(&prim_types->arena, intern, lt_symbol, NECRO_VAR_VAR)),
    //         necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "gt", necro_create_variable_ast(&prim_types->arena, intern, gt_symbol, NECRO_VAR_VAR)),
    //             necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "lte", necro_create_variable_ast(&prim_types->arena, intern, lte_symbol, NECRO_VAR_VAR)),
    //                 necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "gte", necro_create_variable_ast(&prim_types->arena, intern, gte_symbol, NECRO_VAR_VAR)), NULL))));
    // NecroASTNode* ord_instance_ast = necro_create_instance_ast(&prim_types->arena, intern, "Ord", type_conid, NULL, ord_method_list);
    // NecroPrimDef* ord_instance_def = necro_prim_def_instance(prim_types, intern, ord_instance_ast);
}

void necro_init_prim_defs(NecroPrimTypes* prim_types, NecroIntern* intern)
{
    // NecroVal
    NecroASTNode* necro_val_s_type   = necro_create_simple_type_ast(&prim_types->arena, intern, "_NecroVal", NULL);
    NecroPrimDef* necro_val_data_def = necro_prim_def_data(prim_types, intern, &prim_types->necro_val_type, necro_create_data_declaration_ast(&prim_types->arena, intern, necro_val_s_type, NULL));

    // NecroData
    NecroASTNode* necro_data_s_type   = necro_create_simple_type_ast(&prim_types->arena, intern, "NecroData#", NULL);
    NecroPrimDef* necro_data_data_def = necro_prim_def_data(prim_types, intern, &prim_types->necro_data_type, necro_create_data_declaration_ast(&prim_types->arena, intern, necro_data_s_type, NULL));

    // Any
    NecroASTNode* any_s_type           = necro_create_simple_type_ast(&prim_types->arena, intern, "Any#", NULL);
    NecroPrimDef* any_data_def         = necro_prim_def_data(prim_types, intern, &prim_types->any_type, necro_create_data_declaration_ast(&prim_types->arena, intern, any_s_type, NULL));

    // Int#
    NecroASTNode* unboxed_int_s_type   = necro_create_simple_type_ast(&prim_types->arena, intern, "Int#", NULL);
    NecroPrimDef* unboxed_int_data_def = necro_prim_def_data(prim_types, intern, &prim_types->unboxed_int_type, necro_create_data_declaration_ast(&prim_types->arena, intern, unboxed_int_s_type, NULL));

    // Float#
    NecroASTNode* unboxed_float_s_type   = necro_create_simple_type_ast(&prim_types->arena, intern, "Float#", NULL);
    NecroPrimDef* unboxed_float_data_def = necro_prim_def_data(prim_types, intern, &prim_types->unboxed_float_type, necro_create_data_declaration_ast(&prim_types->arena, intern, unboxed_float_s_type, NULL));

    // ()
    NecroASTNode* unit_s_type           = necro_create_simple_type_ast(&prim_types->arena, intern, "()", NULL);
    NecroASTNode* unit_constructor      = necro_create_data_constructor_ast(&prim_types->arena, intern, "()", NULL);
    NecroASTNode* unit_constructor_list = necro_create_ast_list(&prim_types->arena, unit_constructor, NULL);
    NecroPrimDef* unit_data_def         = necro_prim_def_data(prim_types, intern, &prim_types->unit_type, necro_create_data_declaration_ast(&prim_types->arena, intern, unit_s_type, unit_constructor_list));

    // []
    NecroASTNode* list_s_type           = necro_create_simple_type_ast(&prim_types->arena, intern, "[]", necro_create_var_list_ast(&prim_types->arena, intern, 1, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroASTNode* list_app              = necro_create_type_app_ast(&prim_types->arena, necro_create_conid_ast(&prim_types->arena, intern, "[]", NECRO_CON_TYPE_VAR), necro_create_variable_ast(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR));
    NecroASTNode* cons_args             = necro_create_ast_list(&prim_types->arena, necro_create_variable_ast(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR), necro_create_ast_list(&prim_types->arena, list_app, NULL));
    NecroASTNode* cons_constructor      = necro_create_data_constructor_ast(&prim_types->arena, intern, ":", cons_args);
    NecroASTNode* nil_constructor       = necro_create_data_constructor_ast(&prim_types->arena, intern, "[]", NULL);
    NecroASTNode* list_constructor_list = necro_create_ast_list(&prim_types->arena, cons_constructor, necro_create_ast_list(&prim_types->arena, nil_constructor, NULL));
    NecroPrimDef* list_data_def         = necro_prim_def_data(prim_types, intern, &prim_types->list_type, necro_create_data_declaration_ast(&prim_types->arena, intern, list_s_type, list_constructor_list));

    // Sequence
    NecroASTNode* sequence_s_type       = necro_create_simple_type_ast(&prim_types->arena, intern, "Sequence", necro_create_var_list_ast(&prim_types->arena, intern, 1, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroASTNode* sequence_app          = necro_create_type_app_ast(&prim_types->arena, necro_create_conid_ast(&prim_types->arena, intern, "Sequence", NECRO_CON_TYPE_VAR), necro_create_variable_ast(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR));
    // NecroASTNode* cons_args             = necro_create_ast_list(&prim_types->arena, necro_create_variable_ast(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR), necro_create_ast_list(&prim_types->arena, list_app, NULL));
    // NecroASTNode* cons_constructor      = necro_create_data_constructor_ast(&prim_types->arena, intern, ":", cons_args);
    // NecroASTNode* nil_constructor       = necro_create_data_constructor_ast(&prim_types->arena, intern, "[]", NULL);
    // NecroASTNode* list_constructor_list = necro_create_ast_list(&prim_types->arena, cons_constructor, necro_create_ast_list(&prim_types->arena, nil_constructor, NULL));
    NecroPrimDef* sequence_data_def     = necro_prim_def_data(prim_types, intern, &prim_types->sequence_type, necro_create_data_declaration_ast(&prim_types->arena, intern, sequence_s_type, NULL));
    NecroASTNode* seq_a_ast             = necro_create_variable_ast(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
    NecroASTNode* seq_type_ast          = necro_create_fun_ast(&prim_types->arena, necro_create_type_app_ast(&prim_types->arena, necro_create_conid_ast(&prim_types->arena, intern, "Sequence", NECRO_CON_TYPE_VAR), seq_a_ast), seq_a_ast);
    NecroPrimDef* seq_type_def          = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, "seq", NULL, seq_type_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    // IO
    NecroASTNode* io_s_type   = necro_create_simple_type_ast(&prim_types->arena, intern, "IO", necro_create_var_list_ast(&prim_types->arena, intern, 1, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroPrimDef* io_data_def = necro_prim_def_data(prim_types, intern, &prim_types->io_type, necro_create_data_declaration_ast(&prim_types->arena, intern, io_s_type, NULL));

    // Event
    NecroASTNode* event_s_type   = necro_create_simple_type_ast(&prim_types->arena, intern, "Event", necro_create_var_list_ast(&prim_types->arena, intern, 1, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroPrimDef* event_data_def = necro_prim_def_data(prim_types, intern, &prim_types->event_type, necro_create_data_declaration_ast(&prim_types->arena, intern, event_s_type, NULL));

    // Pattern
    NecroASTNode* pattern_s_type   = necro_create_simple_type_ast(&prim_types->arena, intern, "Pattern", necro_create_var_list_ast(&prim_types->arena, intern, 1, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroPrimDef* pattern_data_def = necro_prim_def_data(prim_types, intern, &prim_types->pattern_type, necro_create_data_declaration_ast(&prim_types->arena, intern, pattern_s_type, NULL));

    // Bool
    NecroASTNode* bool_s_type           = necro_create_simple_type_ast(&prim_types->arena, intern, "Bool", NULL);
    NecroASTNode* true_constructor      = necro_create_data_constructor_ast(&prim_types->arena, intern, "True", NULL);
    NecroASTNode* false_constructor     = necro_create_data_constructor_ast(&prim_types->arena, intern, "False", NULL);
    NecroASTNode* bool_constructor_list = necro_create_ast_list(&prim_types->arena, true_constructor, necro_create_ast_list(&prim_types->arena, false_constructor, NULL));
    NecroPrimDef* bool_data_def         = necro_prim_def_data(prim_types, intern, &prim_types->bool_type, necro_create_data_declaration_ast(&prim_types->arena, intern, bool_s_type, bool_constructor_list));
    NecroASTNode* bool_type_conid       = necro_create_conid_ast(&prim_types->arena, intern, "Bool", NECRO_CON_TYPE_VAR);
    NecroASTNode* bool_comp_sig_ast     = necro_create_fun_type_sig_ast(&prim_types->arena, intern, "and", NULL, necro_create_fun_ast(&prim_types->arena, bool_type_conid, necro_create_fun_ast(&prim_types->arena, bool_type_conid, bool_type_conid)), NECRO_VAR_DECLARATION, NECRO_SIG_DECLARATION);

    // Eq
    NecroASTNode* eq_method_sig  = necro_create_class_comp_sig(&prim_types->arena, intern, "eq");
    NecroASTNode* neq_method_sig = necro_create_class_comp_sig(&prim_types->arena, intern, "neq");
    NecroASTNode* eq_method_list = necro_create_declaration_list(&prim_types->arena, eq_method_sig, necro_create_declaration_list(&prim_types->arena, neq_method_sig, NULL));
    NecroASTNode* eq_class_ast   = necro_create_type_class_ast(&prim_types->arena, intern, "Eq", "a", NULL, eq_method_list);
    NecroPrimDef* eq_class_def   = necro_prim_def_class(prim_types, intern, &prim_types->eq_type_class, eq_class_ast);

    // Ord
    NecroASTNode* gt_method_sig   = necro_create_class_comp_sig(&prim_types->arena, intern, "gt");
    NecroASTNode* lt_method_sig   = necro_create_class_comp_sig(&prim_types->arena, intern, "lt");
    NecroASTNode* gte_method_sig  = necro_create_class_comp_sig(&prim_types->arena, intern, "gte");
    NecroASTNode* lte_method_sig  = necro_create_class_comp_sig(&prim_types->arena, intern, "lte");
    NecroASTNode* ord_method_list =
        necro_create_declaration_list(&prim_types->arena, gt_method_sig,
            necro_create_declaration_list(&prim_types->arena, lt_method_sig,
                necro_create_declaration_list(&prim_types->arena, gte_method_sig,
                    necro_create_declaration_list(&prim_types->arena, lte_method_sig, NULL))));
    NecroASTNode* ord_class_ast   = necro_create_type_class_ast(&prim_types->arena, intern, "Ord", "a", necro_create_context(&prim_types->arena, intern, "Eq", "a", NULL), ord_method_list);
    NecroPrimDef* ord_class_def   = necro_prim_def_class(prim_types, intern, &prim_types->ord_type_class, ord_class_ast);

    // Prim Numbers
    necro_create_prim_num(prim_types, intern, &prim_types->int_type, "Int");
    necro_create_prim_num(prim_types, intern, &prim_types->float_type, "Float");
    necro_create_prim_num(prim_types, intern, &prim_types->audio_type, "Audio");
    necro_create_prim_num(prim_types, intern, &prim_types->rational_type, "Rational");

    // Num
    NecroASTNode* add_method_sig    = necro_create_class_bin_op_sig(&prim_types->arena, intern, "add");
    NecroASTNode* sub_method_sig    = necro_create_class_bin_op_sig(&prim_types->arena, intern, "sub");
    NecroASTNode* mul_method_sig    = necro_create_class_bin_op_sig(&prim_types->arena, intern, "mul");
    NecroASTNode* abs_method_sig    = necro_create_class_unary_op_sig(&prim_types->arena, intern, "abs");
    NecroASTNode* signum_method_sig = necro_create_class_unary_op_sig(&prim_types->arena, intern, "signum");
    NecroASTNode* from_int_sig      =
        necro_create_fun_type_sig_ast(&prim_types->arena, intern, "fromInt", NULL, necro_create_fun_ast(&prim_types->arena,
            necro_create_conid_ast(&prim_types->arena, intern, "Int", NECRO_CON_TYPE_VAR),
            necro_create_variable_ast(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR)), NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
    NecroASTNode* num_method_list =
        necro_create_declaration_list(&prim_types->arena, add_method_sig,
            necro_create_declaration_list(&prim_types->arena, sub_method_sig,
                necro_create_declaration_list(&prim_types->arena, mul_method_sig,
                    necro_create_declaration_list(&prim_types->arena, abs_method_sig,
                        necro_create_declaration_list(&prim_types->arena, signum_method_sig,
                            necro_create_declaration_list(&prim_types->arena, from_int_sig, NULL))))));
    NecroASTNode* num_class_ast = necro_create_type_class_ast(&prim_types->arena, intern, "Num", "a", NULL, num_method_list);
    NecroPrimDef* num_class_def = necro_prim_def_class(prim_types, intern, &prim_types->num_type_class, num_class_ast);

    // Frac
    NecroASTNode* div_method_sig    = necro_create_class_bin_op_sig(&prim_types->arena, intern, "div");
    NecroASTNode* recip_method_sig  = necro_create_class_unary_op_sig(&prim_types->arena, intern, "recip");
    NecroASTNode* from_rational_sig =
        necro_create_fun_type_sig_ast(&prim_types->arena, intern, "fromRational", NULL, necro_create_fun_ast(&prim_types->arena,
            necro_create_conid_ast(&prim_types->arena, intern, "Rational", NECRO_CON_TYPE_VAR),
            necro_create_variable_ast(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR)), NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
    NecroASTNode* fractional_method_list =
        necro_create_declaration_list(&prim_types->arena, div_method_sig,
            necro_create_declaration_list(&prim_types->arena, recip_method_sig,
                necro_create_declaration_list(&prim_types->arena, from_rational_sig, NULL)));
    NecroASTNode* fractional_class_ast = necro_create_type_class_ast(&prim_types->arena, intern, "Fractional", "a", necro_create_context(&prim_types->arena, intern, "Num", "a", NULL), fractional_method_list);
    NecroPrimDef* fractional_class_def = necro_prim_def_class(prim_types, intern, &prim_types->fractional_type_class, fractional_class_ast);

    // Prim Num Instances
    necro_create_prim_num_instances(prim_types, intern, "Pattern", true, true);
    necro_create_prim_num_instances(prim_types, intern, "Int", false, false);
    necro_create_prim_num_instances(prim_types, intern, "Float", false, true);
    necro_create_prim_num_instances(prim_types, intern, "Audio", false, true);
    necro_create_prim_num_instances(prim_types, intern, "Rational", false, true);

    // (,)
    NecroASTNode* tuple_2_s_type      = necro_create_simple_type_ast(&prim_types->arena, intern, "(,)", necro_create_var_list_ast(&prim_types->arena, intern, 2, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroASTNode* tuple_2_constructor = necro_create_data_constructor_ast(&prim_types->arena, intern, "(,)", necro_create_var_list_ast(&prim_types->arena, intern, 2, NECRO_VAR_TYPE_FREE_VAR));
    NecroPrimDef* tuple_2_data_def    = necro_prim_def_data(prim_types, intern, &prim_types->tuple_types.two, necro_create_data_declaration_ast(&prim_types->arena, intern, tuple_2_s_type, necro_create_ast_list(&prim_types->arena, tuple_2_constructor, NULL)));

    // (,,)
    NecroASTNode* tuple_3_s_type      = necro_create_simple_type_ast(&prim_types->arena, intern, "(,,)", necro_create_var_list_ast(&prim_types->arena, intern, 3, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroASTNode* tuple_3_constructor = necro_create_data_constructor_ast(&prim_types->arena, intern, "(,,)", necro_create_var_list_ast(&prim_types->arena, intern, 3, NECRO_VAR_TYPE_FREE_VAR));
    NecroPrimDef* tuple_3_data_def    = necro_prim_def_data(prim_types, intern, &prim_types->tuple_types.three, necro_create_data_declaration_ast(&prim_types->arena, intern, tuple_3_s_type, necro_create_ast_list(&prim_types->arena, tuple_3_constructor, NULL)));

    // (,,,)
    NecroASTNode* tuple_4_s_type      = necro_create_simple_type_ast(&prim_types->arena, intern, "(,,,)", necro_create_var_list_ast(&prim_types->arena, intern, 4, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroASTNode* tuple_4_constructor = necro_create_data_constructor_ast(&prim_types->arena, intern, "(,,,)", necro_create_var_list_ast(&prim_types->arena, intern, 4, NECRO_VAR_TYPE_FREE_VAR));
    NecroPrimDef* tuple_4_data_def    = necro_prim_def_data(prim_types, intern, &prim_types->tuple_types.four, necro_create_data_declaration_ast(&prim_types->arena, intern, tuple_4_s_type, necro_create_ast_list(&prim_types->arena, tuple_4_constructor, NULL)));

    // (,,,,)
    NecroASTNode* tuple_5_s_type      = necro_create_simple_type_ast(&prim_types->arena, intern, "(,,,,)", necro_create_var_list_ast(&prim_types->arena, intern, 5, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroASTNode* tuple_5_constructor = necro_create_data_constructor_ast(&prim_types->arena, intern, "(,,,,)", necro_create_var_list_ast(&prim_types->arena, intern, 5, NECRO_VAR_TYPE_FREE_VAR));
    NecroPrimDef* tuple_5_data_def    = necro_prim_def_data(prim_types, intern, &prim_types->tuple_types.five, necro_create_data_declaration_ast(&prim_types->arena, intern, tuple_5_s_type, necro_create_ast_list(&prim_types->arena, tuple_5_constructor, NULL)));

    // (,,,,,)
    NecroASTNode* tuple_6_s_type      = necro_create_simple_type_ast(&prim_types->arena, intern, "(,,,,,)", necro_create_var_list_ast(&prim_types->arena, intern, 6, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroASTNode* tuple_6_constructor = necro_create_data_constructor_ast(&prim_types->arena, intern, "(,,,,,)", necro_create_var_list_ast(&prim_types->arena, intern, 6, NECRO_VAR_TYPE_FREE_VAR));
    NecroPrimDef* tuple_6_data_def    = necro_prim_def_data(prim_types, intern, &prim_types->tuple_types.six, necro_create_data_declaration_ast(&prim_types->arena, intern, tuple_6_s_type, necro_create_ast_list(&prim_types->arena, tuple_6_constructor, NULL)));

    // (,,,,,,)
    NecroASTNode* tuple_7_s_type      = necro_create_simple_type_ast(&prim_types->arena, intern, "(,,,,,,)", necro_create_var_list_ast(&prim_types->arena, intern, 7, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroASTNode* tuple_7_constructor = necro_create_data_constructor_ast(&prim_types->arena, intern, "(,,,,,,)", necro_create_var_list_ast(&prim_types->arena, intern, 7, NECRO_VAR_TYPE_FREE_VAR));
    NecroPrimDef* tuple_7_data_def    = necro_prim_def_data(prim_types, intern, &prim_types->tuple_types.seven, necro_create_data_declaration_ast(&prim_types->arena, intern, tuple_7_s_type, necro_create_ast_list(&prim_types->arena, tuple_7_constructor, NULL)));

    // (,,,,,,,)
    NecroASTNode* tuple_8_s_type      = necro_create_simple_type_ast(&prim_types->arena, intern, "(,,,,,,,)", necro_create_var_list_ast(&prim_types->arena, intern, 8, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroASTNode* tuple_8_constructor = necro_create_data_constructor_ast(&prim_types->arena, intern, "(,,,,,,,)", necro_create_var_list_ast(&prim_types->arena, intern, 8, NECRO_VAR_TYPE_FREE_VAR));
    NecroPrimDef* tuple_8_data_def    = necro_prim_def_data(prim_types, intern, &prim_types->tuple_types.eight, necro_create_data_declaration_ast(&prim_types->arena, intern, tuple_8_s_type, necro_create_ast_list(&prim_types->arena, tuple_8_constructor, NULL)));

    // (,,,,,,,,)
    NecroASTNode* tuple_9_s_type      = necro_create_simple_type_ast(&prim_types->arena, intern, "(,,,,,,,,)", necro_create_var_list_ast(&prim_types->arena, intern, 9, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroASTNode* tuple_9_constructor = necro_create_data_constructor_ast(&prim_types->arena, intern, "(,,,,,,,,)", necro_create_var_list_ast(&prim_types->arena, intern, 9, NECRO_VAR_TYPE_FREE_VAR));
    NecroPrimDef* tuple_9_data_def    = necro_prim_def_data(prim_types, intern, &prim_types->tuple_types.nine, necro_create_data_declaration_ast(&prim_types->arena, intern, tuple_9_s_type, necro_create_ast_list(&prim_types->arena, tuple_9_constructor, NULL)));

    // (,,,,,,,,,)
    NecroASTNode* tuple_10_s_type      = necro_create_simple_type_ast(&prim_types->arena, intern, "(,,,,,,,,,)", necro_create_var_list_ast(&prim_types->arena, intern, 10, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroASTNode* tuple_10_constructor = necro_create_data_constructor_ast(&prim_types->arena, intern, "(,,,,,,,,,)", necro_create_var_list_ast(&prim_types->arena, intern, 10, NECRO_VAR_TYPE_FREE_VAR));
    NecroPrimDef* tuple_10_data_def    = necro_prim_def_data(prim_types, intern, &prim_types->tuple_types.ten, necro_create_data_declaration_ast(&prim_types->arena, intern, tuple_10_s_type, necro_create_ast_list(&prim_types->arena, tuple_10_constructor, NULL)));

    // Functor
    {
        NecroASTNode* a_var          = necro_create_variable_ast(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* b_var          = necro_create_variable_ast(&prim_types->arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* f_var          = necro_create_variable_ast(&prim_types->arena, intern, "f", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* map_method_sig =
            necro_create_fun_type_sig_ast(&prim_types->arena, intern, "map", NULL,
                necro_create_fun_ast(&prim_types->arena,
                    necro_create_fun_ast(&prim_types->arena, a_var, b_var),
                    necro_create_fun_ast(&prim_types->arena,
                        necro_create_type_app_ast(&prim_types->arena, f_var, a_var),
                        necro_create_type_app_ast(&prim_types->arena, f_var, b_var))),
                    NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
        NecroASTNode* functor_method_list = necro_create_declaration_list(&prim_types->arena, map_method_sig, NULL);
        NecroASTNode* functor_class_ast   = necro_create_type_class_ast(&prim_types->arena, intern, "Functor", "f", NULL, functor_method_list);
        NecroPrimDef* functor_class_def   = necro_prim_def_class(prim_types, intern, &prim_types->functor_type_class, functor_class_ast);
    }

    // Applicative
    {
        NecroASTNode* a_var           = necro_create_variable_ast(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* b_var           = necro_create_variable_ast(&prim_types->arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* f_var           = necro_create_variable_ast(&prim_types->arena, intern, "f", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* pure_method_sig =
            necro_create_fun_type_sig_ast(&prim_types->arena, intern, "pure", NULL, necro_create_fun_ast(&prim_types->arena, a_var, necro_create_type_app_ast(&prim_types->arena, f_var, a_var)), NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
        NecroASTNode* ap_method_sig  =
            necro_create_fun_type_sig_ast(&prim_types->arena, intern, "ap", NULL,
                necro_create_fun_ast(&prim_types->arena,
                    necro_create_type_app_ast(&prim_types->arena, f_var, necro_create_fun_ast(&prim_types->arena, a_var, b_var)),
                    necro_create_fun_ast(&prim_types->arena,
                        necro_create_type_app_ast(&prim_types->arena, f_var, a_var),
                        necro_create_type_app_ast(&prim_types->arena, f_var, b_var))),
                    NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
        NecroASTNode* applicative_method_list = necro_create_declaration_list(&prim_types->arena, pure_method_sig, necro_create_declaration_list(&prim_types->arena, ap_method_sig, NULL));
        NecroASTNode* applicative_class_ast   = necro_create_type_class_ast(&prim_types->arena, intern, "Applicative", "f", necro_create_context(&prim_types->arena, intern, "Functor", "f", NULL), applicative_method_list);
        NecroPrimDef* applicative_class_def   = necro_prim_def_class(prim_types, intern, &prim_types->applicative_type_class, applicative_class_ast);
    }

    // Monad
    {
        NecroASTNode* a_var           = necro_create_variable_ast(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* b_var           = necro_create_variable_ast(&prim_types->arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* m_var           = necro_create_variable_ast(&prim_types->arena, intern, "m", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* bind_method_sig  =
            necro_create_fun_type_sig_ast(&prim_types->arena, intern, "bind", NULL,
                necro_create_fun_ast(&prim_types->arena,
                    necro_create_type_app_ast(&prim_types->arena, m_var, a_var),
                    necro_create_fun_ast(&prim_types->arena,
                        necro_create_fun_ast(&prim_types->arena, a_var, necro_create_type_app_ast(&prim_types->arena, m_var, b_var)),
                        necro_create_type_app_ast(&prim_types->arena, m_var, b_var))),
                    NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
        NecroASTNode* monad_method_list = necro_create_declaration_list(&prim_types->arena, bind_method_sig, NULL);
        NecroASTNode* monad_class_ast   = necro_create_type_class_ast(&prim_types->arena, intern, "Monad", "m", necro_create_context(&prim_types->arena, intern, "Applicative", "m", NULL), monad_method_list);
        NecroPrimDef* monad_class_def   = necro_prim_def_class(prim_types, intern, &prim_types->monad_type_class, monad_class_ast);


    }

    // Semigroup
    {
        NecroASTNode* a_var             = necro_create_variable_ast(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* append_method_sig =
            necro_create_fun_type_sig_ast(&prim_types->arena, intern, "append", NULL,
                necro_create_fun_ast(&prim_types->arena, a_var, necro_create_fun_ast(&prim_types->arena, a_var, a_var)),
                    NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
        NecroASTNode* semigroup_method_list = necro_create_declaration_list(&prim_types->arena, append_method_sig, NULL);
        NecroASTNode* semigroup_class_ast   = necro_create_type_class_ast(&prim_types->arena, intern, "Semigroup", "a", NULL, semigroup_method_list);
        NecroPrimDef* semigroup_class_def   = necro_prim_def_class(prim_types, intern, NULL, semigroup_class_ast);
    }

    // Monoid
    {
        NecroASTNode* a_var             = necro_create_variable_ast(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* mempty_method_sig =
            necro_create_fun_type_sig_ast(&prim_types->arena, intern, "mempty", NULL,
                a_var,
                    NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
        NecroASTNode* monoid_method_list = necro_create_declaration_list(&prim_types->arena, mempty_method_sig, NULL);
        NecroASTNode* monoid_class_ast   = necro_create_type_class_ast(&prim_types->arena, intern, "Monoid", "a", necro_create_context(&prim_types->arena, intern, "Semigroup", "a", NULL), monoid_method_list);
        NecroPrimDef* monoid_class_def   = necro_prim_def_class(prim_types, intern, NULL, monoid_class_ast);
    }

    //=====================================================
    // BinOps
    //=====================================================

    // +
    NecroASTNode* add_sig_ast  = necro_create_num_bin_op_sig(&prim_types->arena, intern, "+");
    NecroASTNode* add_def_ast  = necro_create_simple_assignment(&prim_types->arena, intern, "+", necro_create_variable_ast(&prim_types->arena, intern, "add", NECRO_VAR_VAR));
    NecroPrimDef* add_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, add_sig_ast, add_def_ast);

    // -
    NecroASTNode* sub_sig_ast  = necro_create_num_bin_op_sig(&prim_types->arena, intern, "-");
    NecroASTNode* sub_def_ast  = necro_create_simple_assignment(&prim_types->arena, intern, "-", necro_create_variable_ast(&prim_types->arena, intern, "sub", NECRO_VAR_VAR));
    NecroPrimDef* sub_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, sub_sig_ast, sub_def_ast);

    // *
    NecroASTNode* mul_sig_ast  = necro_create_num_bin_op_sig(&prim_types->arena, intern, "*");
    NecroASTNode* mul_def_ast  = necro_create_simple_assignment(&prim_types->arena, intern, "*", necro_create_variable_ast(&prim_types->arena, intern, "mul", NECRO_VAR_VAR));
    NecroPrimDef* mul_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, mul_sig_ast, mul_def_ast);

    // /
    NecroASTNode* div_sig_ast  = necro_create_fractional_bin_op_sig(&prim_types->arena, intern, "/");
    NecroASTNode* div_def_ast  = necro_create_simple_assignment(&prim_types->arena, intern, "/", necro_create_variable_ast(&prim_types->arena, intern, "div", NECRO_VAR_VAR));
    NecroPrimDef* div_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, div_sig_ast, div_def_ast);

    // ==
    NecroASTNode* eq_sig_ast  = necro_create_eq_comp_sig(&prim_types->arena, intern, "==");
    NecroASTNode* eq_def_ast  = necro_create_simple_assignment(&prim_types->arena, intern, "==", necro_create_variable_ast(&prim_types->arena, intern, "eq", NECRO_VAR_VAR));
    NecroPrimDef* eq_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, eq_sig_ast, eq_def_ast);

    // /=
    NecroASTNode* neq_sig_ast  = necro_create_eq_comp_sig(&prim_types->arena, intern, "/=");
    NecroASTNode* neq_def_ast  = necro_create_simple_assignment(&prim_types->arena, intern, "/=", necro_create_variable_ast(&prim_types->arena, intern, "neq", NECRO_VAR_VAR));
    NecroPrimDef* neq_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, neq_sig_ast, neq_def_ast);

    // <
    NecroASTNode* lt_sig_ast  = necro_create_ord_comp_sig(&prim_types->arena, intern, "<");
    NecroASTNode* lt_def_ast  = necro_create_simple_assignment(&prim_types->arena, intern, "<", necro_create_variable_ast(&prim_types->arena, intern, "lt", NECRO_VAR_VAR));
    NecroPrimDef* lt_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, lt_sig_ast, lt_def_ast);

    // >
    NecroASTNode* gt_sig_ast  = necro_create_ord_comp_sig(&prim_types->arena, intern, ">");
    NecroASTNode* gt_def_ast  = necro_create_simple_assignment(&prim_types->arena, intern, ">", necro_create_variable_ast(&prim_types->arena, intern, "gt", NECRO_VAR_VAR));
    NecroPrimDef* gt_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, gt_sig_ast, gt_def_ast);

    // <=
    NecroASTNode* lte_sig_ast  = necro_create_ord_comp_sig(&prim_types->arena, intern, "<=");
    NecroASTNode* lte_def_ast  = necro_create_simple_assignment(&prim_types->arena, intern, "<=", necro_create_variable_ast(&prim_types->arena, intern, "lte", NECRO_VAR_VAR));
    NecroPrimDef* lte_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, lte_sig_ast, lte_def_ast);

    // >=
    NecroASTNode* gte_sig_ast  = necro_create_ord_comp_sig(&prim_types->arena, intern, ">=");
    NecroASTNode* gte_def_ast  = necro_create_simple_assignment(&prim_types->arena, intern, ">=", necro_create_variable_ast(&prim_types->arena, intern, "gte", NECRO_VAR_VAR));
    NecroPrimDef* gte_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, gte_sig_ast, gte_def_ast);

    // >>=
    {
        NecroASTNode* a_var       = necro_create_variable_ast(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* b_var       = necro_create_variable_ast(&prim_types->arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* m_var       = necro_create_variable_ast(&prim_types->arena, intern, "m", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* bind_op_sig =
            necro_create_fun_type_sig_ast(&prim_types->arena, intern, ">>=", necro_create_context(&prim_types->arena, intern, "Monad", "m", NULL),
                necro_create_fun_ast(&prim_types->arena,
                    necro_create_type_app_ast(&prim_types->arena, m_var, a_var),
                    necro_create_fun_ast(&prim_types->arena,
                        necro_create_fun_ast(&prim_types->arena, a_var, necro_create_type_app_ast(&prim_types->arena, m_var, b_var)),
                        necro_create_type_app_ast(&prim_types->arena, m_var, b_var))),
                NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        NecroASTNode* bind_def_ast = necro_create_simple_assignment(&prim_types->arena, intern, ">>=", necro_create_variable_ast(&prim_types->arena, intern, "bind", NECRO_VAR_VAR));
        NecroPrimDef* bind_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, bind_op_sig, bind_def_ast);
    }

    // >>
    {
        NecroASTNode* a_var       = necro_create_variable_ast(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* b_var       = necro_create_variable_ast(&prim_types->arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* m_var       = necro_create_variable_ast(&prim_types->arena, intern, "m", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* op_sig =
            necro_create_fun_type_sig_ast(&prim_types->arena, intern, ">>", necro_create_context(&prim_types->arena, intern, "Monad", "m", NULL),
                necro_create_fun_ast(&prim_types->arena,
                    necro_create_type_app_ast(&prim_types->arena, m_var, a_var),
                    necro_create_fun_ast(&prim_types->arena,
                        necro_create_type_app_ast(&prim_types->arena, m_var, b_var),
                        necro_create_type_app_ast(&prim_types->arena, m_var, b_var))),
                NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        NecroASTNode* mm_var      = necro_create_variable_ast(&prim_types->arena, intern, "m", NECRO_VAR_DECLARATION);
        NecroASTNode* mk_var      = necro_create_variable_ast(&prim_types->arena, intern, "k", NECRO_VAR_DECLARATION);
        NecroASTNode* op_args     = necro_create_apat_list_ast(&prim_types->arena, mm_var, necro_create_apat_list_ast(&prim_types->arena, mk_var, NULL));
        NecroASTNode* op_lambda   = necro_create_bin_op_ast(&prim_types->arena, intern, ">>=",
            necro_create_variable_ast(&prim_types->arena, intern, "m", NECRO_VAR_VAR),
            necro_create_lambda_ast(&prim_types->arena, necro_create_apat_list_ast(&prim_types->arena, necro_create_wild_card_ast(&prim_types->arena), NULL), necro_create_variable_ast(&prim_types->arena, intern, "k", NECRO_VAR_VAR)));
        NecroASTNode* op_rhs      = necro_create_rhs_ast(&prim_types->arena, op_lambda, NULL);
        NecroASTNode* op_def_ast  = necro_create_apats_assignment_ast(&prim_types->arena, intern, ">>", op_args, op_rhs);
        NecroPrimDef* op_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, op_sig, op_def_ast);
    }

    // <>
    {
        NecroASTNode* a_var         = necro_create_variable_ast(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* append_op_sig =
            necro_create_fun_type_sig_ast(&prim_types->arena, intern, "<>", necro_create_context(&prim_types->arena, intern, "Semigroup", "a", NULL),
                necro_create_fun_ast(&prim_types->arena, a_var, necro_create_fun_ast(&prim_types->arena, a_var, a_var)),
                NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        NecroASTNode* append_def_ast  = necro_create_simple_assignment(&prim_types->arena, intern, "<>", necro_create_variable_ast(&prim_types->arena, intern, "append", NECRO_VAR_VAR));
        NecroPrimDef* append_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, append_op_sig, append_def_ast);
    }

    // <|
    {
        NecroASTNode* a_var       = necro_create_variable_ast(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* b_var       = necro_create_variable_ast(&prim_types->arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* op_sig      =
            necro_create_fun_type_sig_ast(&prim_types->arena, intern, "<|", NULL,
                necro_create_fun_ast(&prim_types->arena, necro_create_fun_ast(&prim_types->arena, a_var, b_var),
                    necro_create_fun_ast(&prim_types->arena, a_var, b_var)),
                NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        NecroASTNode* f_var       = necro_create_variable_ast(&prim_types->arena, intern, "f", NECRO_VAR_DECLARATION);
        NecroASTNode* x_var       = necro_create_variable_ast(&prim_types->arena, intern, "x", NECRO_VAR_DECLARATION);
        NecroASTNode* op_args     = necro_create_apat_list_ast(&prim_types->arena, f_var, necro_create_apat_list_ast(&prim_types->arena, x_var, NULL));
        NecroASTNode* op_rhs_ast  = necro_create_rhs_ast(&prim_types->arena, necro_create_fexpr_ast(&prim_types->arena, necro_create_variable_ast(&prim_types->arena, intern, "f", NECRO_VAR_VAR), necro_create_variable_ast(&prim_types->arena, intern, "x", NECRO_VAR_VAR)), NULL);
        NecroASTNode* op_def_ast  = necro_create_apats_assignment_ast(&prim_types->arena, intern, "<|", op_args, op_rhs_ast);
        NecroPrimDef* op_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, op_sig, op_def_ast);
    }

    // |>
    {
        NecroASTNode* a_var       = necro_create_variable_ast(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* b_var       = necro_create_variable_ast(&prim_types->arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* op_sig      =
            necro_create_fun_type_sig_ast(&prim_types->arena, intern, "|>", NULL,
                necro_create_fun_ast(&prim_types->arena, a_var,
                    necro_create_fun_ast(&prim_types->arena, necro_create_fun_ast(&prim_types->arena, a_var, b_var), b_var)),
                NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        NecroASTNode* x_var       = necro_create_variable_ast(&prim_types->arena, intern, "x", NECRO_VAR_DECLARATION);
        NecroASTNode* f_var       = necro_create_variable_ast(&prim_types->arena, intern, "f", NECRO_VAR_DECLARATION);
        NecroASTNode* op_args     = necro_create_apat_list_ast(&prim_types->arena, x_var, necro_create_apat_list_ast(&prim_types->arena, f_var, NULL));
        NecroASTNode* op_rhs_ast  = necro_create_rhs_ast(&prim_types->arena, necro_create_fexpr_ast(&prim_types->arena, necro_create_variable_ast(&prim_types->arena, intern, "f", NECRO_VAR_VAR), necro_create_variable_ast(&prim_types->arena, intern, "x", NECRO_VAR_VAR)), NULL);
        NecroASTNode* op_def_ast  = necro_create_apats_assignment_ast(&prim_types->arena, intern, "|>", op_args, op_rhs_ast);
        NecroPrimDef* op_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, op_sig, op_def_ast);
    }

    // .
    {
        NecroASTNode* a_var       = necro_create_variable_ast(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* b_var       = necro_create_variable_ast(&prim_types->arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* c_var       = necro_create_variable_ast(&prim_types->arena, intern, "c", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* op_sig      =
            necro_create_fun_type_sig_ast(&prim_types->arena, intern, ".", NULL,
                necro_create_fun_ast(&prim_types->arena, necro_create_fun_ast(&prim_types->arena, b_var, c_var),
                    necro_create_fun_ast(&prim_types->arena, necro_create_fun_ast(&prim_types->arena, a_var, b_var),
                        necro_create_fun_ast(&prim_types->arena, a_var, c_var))),
                NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        NecroASTNode* f_var       = necro_create_variable_ast(&prim_types->arena, intern, "f", NECRO_VAR_DECLARATION);
        NecroASTNode* g_var       = necro_create_variable_ast(&prim_types->arena, intern, "g", NECRO_VAR_DECLARATION);
        NecroASTNode* x_var       = necro_create_variable_ast(&prim_types->arena, intern, "x", NECRO_VAR_DECLARATION);
        NecroASTNode* op_args     = necro_create_apat_list_ast(&prim_types->arena, f_var, necro_create_apat_list_ast(&prim_types->arena, g_var, NULL));
        NecroASTNode* op_lambda   = necro_create_lambda_ast(&prim_types->arena, necro_create_apat_list_ast(&prim_types->arena, x_var, NULL),
            necro_create_fexpr_ast(&prim_types->arena, necro_create_variable_ast(&prim_types->arena, intern, "f", NECRO_VAR_VAR),
                necro_create_fexpr_ast(&prim_types->arena, necro_create_variable_ast(&prim_types->arena, intern, "g", NECRO_VAR_VAR),
                    necro_create_variable_ast(&prim_types->arena, intern, "x", NECRO_VAR_VAR))));
        NecroASTNode* op_rhs_ast  = necro_create_rhs_ast(&prim_types->arena, op_lambda, NULL);
        NecroASTNode* op_def_ast  = necro_create_apats_assignment_ast(&prim_types->arena, intern, ".", op_args, op_rhs_ast);
        NecroPrimDef* op_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, op_sig, op_def_ast);
    }


    {
        // delay
        NecroASTNode* a_var         = necro_create_variable_ast(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroASTNode* delay_sig_ast = necro_create_fun_type_sig_ast(&prim_types->arena, intern, "delay", NULL,
            necro_create_fun_ast(&prim_types->arena, a_var,
                necro_create_fun_ast(&prim_types->arena, a_var, a_var)), NECRO_VAR_DECLARATION, NECRO_SIG_DECLARATION);
        necro_prim_def_fun(prim_types, intern, &prim_types->delay_fn, delay_sig_ast);
    }

    // TODO: Finish Bool Eq/Ord instances and && / ||!!!
    // &&
    // NecroASTNode* bool_conid   = necro_create_conid_ast(&prim_types->arena, intern, "Bool", NECRO_CON_TYPE_VAR);
    // NecroASTNode* and_sig_ast  =
    //     necro_create_fun_type_sig_ast(&prim_types->arena, intern, "&&", NULL,
    //         necro_create_fun_ast(&prim_types->arena,
    //             bool_conid,
    //             necro_create_fun_ast(&prim_types->arena,
    //                 bool_conid,
    //                 bool_conid)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
    // NecroASTNode* and_def_ast  = necro_create_simple_assignment(&prim_types->arena, intern, "&&", necro_create_variable_ast(&prim_types->arena, intern, "eq@Bool", NECRO_VAR_VAR));
    // NecroPrimDef* and_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, gte_sig_ast, gte_def_ast);
    // NecroPrimDef* and_fun_def           = necro_prim_def_fun(prim_types, intern, &prim_types->bin_op_types.and_type, bool_comp_sig_ast);
    // NecroPrimDef* ord_fun_def           = necro_prim_def_fun(prim_types, intern, &prim_types->bin_op_types.or_type, bool_comp_sig_ast);

}

void necro_gen_bin_op(NecroCodeGen* codegen, NecroVar bin_op_name, LLVMTypeRef input_type, LLVMTypeRef raw_type, LLVMOpcode op_code)
{
    NecroArenaSnapshot snapshot  = necro_get_arena_snapshot(&codegen->snapshot_arena);
    // Setup bin op function
    LLVMTypeRef        args[2]   = { LLVMPointerType(input_type,0), LLVMPointerType(input_type, 0) };
    LLVMValueRef       bin_op    = necro_snapshot_add_function(codegen, necro_intern_get_string(codegen->intern, bin_op_name.symbol), LLVMPointerType(input_type, 0), args, 2);
    LLVMBasicBlockRef  entry     = LLVMAppendBasicBlock(bin_op, "entry");
    LLVMPositionBuilderAtEnd(codegen->builder, entry);
    // Retrieve raw values
    LLVMValueRef       raw_left_ptr  = necro_snapshot_gep(codegen, "raw_left_ptr",  LLVMGetParam(bin_op, 0), 2, (uint32_t[]) { 0, 1 });
    LLVMValueRef       raw_right_ptr = necro_snapshot_gep(codegen, "raw_right_ptr", LLVMGetParam(bin_op, 1), 2, (uint32_t[]) { 0, 1 });
    LLVMValueRef       raw_left      = LLVMBuildLoad(codegen->builder, raw_left_ptr, "raw_left");
    LLVMValueRef       raw_right     = LLVMBuildLoad(codegen->builder, raw_right_ptr, "raw_right");
    LLVMValueRef       raw_result    = LLVMBuildBinOp(codegen->builder, op_code, raw_left, raw_right, "raw_result");
    // Store Raw value in result
    LLVMValueRef       data_ptr      = necro_gen_alloc_boxed_value(codegen, input_type, 0, 0, "data_ptr");
    LLVMValueRef       result_ptr    = necro_snapshot_gep(codegen, "result_ptr", data_ptr, 2, (uint32_t[]) { 0, 1 });
    LLVMBuildStore(codegen->builder, raw_result, result_ptr);
    // return
    LLVMBuildRet(codegen->builder, data_ptr);
    necro_create_prim_node_prototype(codegen, bin_op_name, LLVMPointerType(input_type, 0), bin_op, NECRO_NODE_STATELESS);
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
}

void necro_gen_bin_ops(NecroCodeGen* codegen, NecroVar type_name, LLVMTypeRef input_type, LLVMTypeRef raw_type, uint32_t add_op, uint32_t sub_op, uint32_t mul_op, uint32_t div_op)
{
    NecroSymbol add_symbol = necro_intern_string(codegen->intern, necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]) { "add@", necro_intern_get_string(codegen->intern, type_name.symbol) }));
    NecroID     add_id     = necro_scope_find(codegen->infer->scoped_symtable->top_scope, add_symbol);
    necro_gen_bin_op(codegen, (NecroVar) { .id = add_id, .symbol = add_symbol }, input_type, raw_type, add_op);

    NecroSymbol sub_symbol = necro_intern_string(codegen->intern, necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]) { "sub@", necro_intern_get_string(codegen->intern, type_name.symbol) }));
    NecroID     sub_id     = necro_scope_find(codegen->infer->scoped_symtable->top_scope, sub_symbol);
    necro_gen_bin_op(codegen, (NecroVar) { .id = sub_id, .symbol = sub_symbol }, input_type, raw_type, sub_op);

    NecroSymbol mul_symbol = necro_intern_string(codegen->intern, necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]) { "mul@", necro_intern_get_string(codegen->intern, type_name.symbol) }));
    NecroID     mul_id     = necro_scope_find(codegen->infer->scoped_symtable->top_scope, mul_symbol);
    necro_gen_bin_op(codegen, (NecroVar) { .id = mul_id, .symbol = mul_symbol }, input_type, raw_type, mul_op);

    if (div_op != 0)
    {
        NecroSymbol div_symbol = necro_intern_string(codegen->intern, necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]) { "div@", necro_intern_get_string(codegen->intern, type_name.symbol) }));
        NecroID     div_id     = necro_scope_find(codegen->infer->scoped_symtable->top_scope, div_symbol);
        necro_gen_bin_op(codegen, (NecroVar) { .id = div_id, .symbol = div_symbol }, input_type, raw_type, div_op);
    }
}

void necro_gen_from_cast(NecroCodeGen* codegen, const char* cast_name_string, LLVMTypeRef boxed_type, LLVMTypeRef raw_type, LLVMTypeRef dest_boxed_type, LLVMTypeRef dest_raw_type, uint32_t cast_op_code)
{
    NecroArenaSnapshot snapshot    = necro_get_arena_snapshot(&codegen->snapshot_arena);
    NecroSymbol        cast_symbol = necro_intern_string(codegen->intern, cast_name_string);
    NecroID            cast_id     = necro_scope_find(codegen->infer->scoped_symtable->top_scope, cast_symbol);
    NecroVar           cast_name   = (NecroVar) { .id = cast_id, .symbol = cast_symbol };
    // Setup bin op function
    LLVMTypeRef        args[1] = { LLVMPointerType(boxed_type,0) };
    LLVMValueRef       cast_op = necro_snapshot_add_function(codegen, necro_intern_get_string(codegen->intern, cast_name.symbol), LLVMPointerType(dest_boxed_type, 0), args, 1);
    LLVMBasicBlockRef  entry   = LLVMAppendBasicBlock(cast_op, "entry");
    LLVMPositionBuilderAtEnd(codegen->builder, entry);
    if (boxed_type == dest_boxed_type)
    {
        LLVMBuildRet(codegen->builder, LLVMGetParam(cast_op, 0));
    }
    else
    {
        // Get Value
        LLVMValueRef raw_ptr    = necro_snapshot_gep(codegen, "raw_ptr",  LLVMGetParam(cast_op, 0), 2, (uint32_t[]) { 0, 1 });
        LLVMValueRef raw_value  = LLVMBuildLoad(codegen->builder, raw_ptr, "raw_value");
        LLVMValueRef raw_result = LLVMBuildCast(codegen->builder, cast_op_code, raw_value, dest_raw_type, "raw_result");
        LLVMValueRef data_ptr   = necro_gen_alloc_boxed_value(codegen, dest_boxed_type, 0, 0, "data_ptr");
        LLVMValueRef result_ptr = necro_snapshot_gep(codegen, "result_ptr", data_ptr, 2, (uint32_t[]) { 0, 1 });
        LLVMBuildStore(codegen->builder, raw_result, result_ptr);
        // return
        LLVMBuildRet(codegen->builder, data_ptr);
    }
    necro_create_prim_node_prototype(codegen, cast_name, LLVMPointerType(dest_boxed_type, 0), cast_op, NECRO_NODE_STATELESS);
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
}

LLVMValueRef necro_codegen_box(NecroCodeGen* codegen, const char* type_name, LLVMTypeRef boxed_type, LLVMTypeRef unboxed_type)
{
    NecroArenaSnapshot snapshot   = necro_get_arena_snapshot(&codegen->snapshot_arena);
    const char*        fn_name    = necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]) { "_mk", type_name });
    LLVMTypeRef        args[1]    = { unboxed_type };
    LLVMValueRef       box_fn     = necro_snapshot_add_function(codegen, fn_name, LLVMPointerType(boxed_type, 0), args, 1);
    LLVMBasicBlockRef  entry      = LLVMAppendBasicBlock(box_fn, "entry");
    LLVMPositionBuilderAtEnd(codegen->builder, entry);
    // Store Unboxed value in Box
    LLVMValueRef       box_ptr    = necro_gen_alloc_boxed_value(codegen, boxed_type, 0, 0, "box_ptr");
    LLVMValueRef       result_ptr = necro_snapshot_gep(codegen, "result_ptr", box_ptr, 2, (uint32_t[]) { 0, 1 });
    LLVMBuildStore(codegen->builder, LLVMGetParam(box_fn, 0), result_ptr);
    LLVMBuildRet(codegen->builder, box_ptr);
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
    return box_fn;
}

NECRO_RETURN_CODE necro_codegen_primitives(NecroCodeGen* codegen)
{
    if (necro_is_codegen_error(codegen)) return codegen->error.return_code;
    assert(codegen != NULL);
    NecroPrimTypes* prim_types = codegen->infer->prim_types;
    assert(prim_types != NULL);
    if (prim_types->llvm_mod != NULL)
        return NECRO_SUCCESS;

    // LLVMModuleRef  codegen_mod     = codegen->mod;
    // LLVMBuilderRef codegen_builder = codegen->builder;

    // codegen->mod     = LLVMModuleCreateWithNameInContext("NecroPrim", codegen->context);
    // codegen->builder = LLVMCreateBuilderInContext(codegen->context);

    //=====================================================
    // Runtime and intrinsics
    //=====================================================
    LLVMTypeRef  mem_cpy_args[4] = { LLVMPointerType(LLVMInt32TypeInContext(codegen->context), 0), LLVMPointerType(LLVMInt32TypeInContext(codegen->context), 0), LLVMInt64TypeInContext(codegen->context), LLVMInt1TypeInContext(codegen->context) };
    LLVMTypeRef  mem_cpy_type    = LLVMFunctionType(LLVMVoidTypeInContext(codegen->context), mem_cpy_args, 4, false);
    LLVMValueRef mem_cpy         = LLVMAddFunction(codegen->mod, "llvm.memcpy.p0i32.p0i32.i64", mem_cpy_type);
    codegen->llvm_intrinsics.mem_cpy = mem_cpy;

    LLVMTypeRef  trap_type = LLVMFunctionType(LLVMVoidTypeInContext(codegen->context), NULL, 0, false);
    LLVMValueRef trap_fn   = LLVMAddFunction(codegen->mod, "llvm.trap", trap_type);
    codegen->llvm_intrinsics.trap = trap_fn;

    necro_declare_runtime_functions(codegen->runtime, codegen);

    //=====================================================
    // Primitive Types
    //=====================================================
    // NecroData#
    LLVMTypeRef necro_type_ref = LLVMStructCreateNamed(codegen->context, "_NecroData");
    LLVMTypeRef necro_elems[2] = { LLVMInt32TypeInContext(codegen->context), LLVMInt32TypeInContext(codegen->context) };
    LLVMStructSetBody(necro_type_ref, necro_elems, 2, false);
    necro_symtable_get(codegen->symtable, prim_types->necro_data_type.id)->llvm_type = necro_type_ref;

    // NecroVal
    LLVMTypeRef necro_val_type_ref = LLVMStructCreateNamed(codegen->context, "_NecroVal");
    LLVMTypeRef necro_val_elems[1] = { necro_type_ref };
    LLVMStructSetBody(necro_val_type_ref, necro_val_elems, 1, false);
    necro_symtable_get(codegen->symtable, prim_types->necro_val_type.id)->llvm_type = necro_val_type_ref;

    // Any#
    LLVMTypeRef any_type_ref = LLVMStructCreateNamed(codegen->context, "_Any");
    LLVMTypeRef any_elems[2] = { necro_type_ref, LLVMPointerType(necro_val_type_ref, 0) };
    LLVMStructSetBody(any_type_ref, any_elems, 2, false);
    necro_symtable_get(codegen->symtable, prim_types->any_type.id)->llvm_type = any_type_ref;

    // Int#
    necro_symtable_get(codegen->symtable, prim_types->unboxed_int_type.id)->llvm_type = LLVMInt64TypeInContext(codegen->context);

    // Float#
    necro_symtable_get(codegen->symtable, prim_types->unboxed_float_type.id)->llvm_type = LLVMDoubleTypeInContext(codegen->context);

    // Int
    LLVMTypeRef int_type_ref = LLVMStructCreateNamed(codegen->context, "Int");
    LLVMTypeRef int_elems[2] = { necro_type_ref, LLVMInt64TypeInContext(codegen->context) };
    LLVMStructSetBody(int_type_ref, int_elems, 2, false);
    necro_symtable_get(codegen->symtable, prim_types->int_type.id)->llvm_type = int_type_ref;
    necro_gen_bin_ops(codegen, necro_con_to_var(codegen->infer->prim_types->int_type), int_type_ref, LLVMInt64TypeInContext(codegen->context), LLVMAdd, LLVMSub, LLVMMul, 0);
    necro_gen_from_cast(codegen, "fromInt@Int", int_type_ref, LLVMInt64TypeInContext(codegen->context), int_type_ref, LLVMInt64TypeInContext(codegen->context), LLVMSIToFP);
    NecroNodePrototype* int_prototype = necro_create_prim_node_prototype(codegen, necro_con_to_var(codegen->infer->prim_types->int_type), NULL, NULL, NECRO_NODE_STATELESS);
    int_prototype->mk_function = necro_codegen_box(codegen, "Int", int_type_ref, LLVMInt64TypeInContext(codegen->context));

    // Float
    LLVMTypeRef float_type_ref = LLVMStructCreateNamed(codegen->context, "Float");
    LLVMTypeRef float_elems[2] = { necro_type_ref, LLVMDoubleTypeInContext(codegen->context) };
    LLVMStructSetBody(float_type_ref, float_elems, 2, false);
    necro_symtable_get(codegen->symtable, prim_types->float_type.id)->llvm_type = float_type_ref;
    necro_gen_bin_ops(codegen, necro_con_to_var(codegen->infer->prim_types->float_type), float_type_ref, LLVMFloatTypeInContext(codegen->context), LLVMFAdd, LLVMFSub, LLVMFMul, LLVMFDiv);
    necro_gen_from_cast(codegen, "fromInt@Float", int_type_ref, LLVMInt64TypeInContext(codegen->context), float_type_ref, LLVMDoubleTypeInContext(codegen->context), LLVMSIToFP);
    necro_gen_from_cast(codegen, "fromRational@Float", float_type_ref, LLVMDoubleTypeInContext(codegen->context), float_type_ref, LLVMDoubleTypeInContext(codegen->context), LLVMSIToFP);
    NecroNodePrototype* float_prototype = necro_create_prim_node_prototype(codegen, necro_con_to_var(codegen->infer->prim_types->float_type), NULL, NULL, NECRO_NODE_STATELESS);
    float_prototype->mk_function = necro_codegen_box(codegen, "Float", float_type_ref, LLVMDoubleTypeInContext(codegen->context));

    // Rational
    LLVMTypeRef rational_type_ref = LLVMStructCreateNamed(codegen->context, "Rational");
    LLVMTypeRef rational_elems[3] = { necro_type_ref, LLVMInt64TypeInContext(codegen->context), LLVMInt64TypeInContext(codegen->context) };
    LLVMStructSetBody(rational_type_ref, rational_elems, 3, false);
    necro_symtable_get(codegen->symtable, prim_types->rational_type.id)->llvm_type = rational_type_ref;

    // Audio
    LLVMTypeRef audio_type_ref = LLVMStructCreateNamed(codegen->context, "Audio");
    LLVMTypeRef audio_elems[2] = { necro_type_ref, LLVMPointerType(LLVMDoubleTypeInContext(codegen->context),0) };
    LLVMStructSetBody(audio_type_ref, audio_elems, 2, false);
    necro_symtable_get(codegen->symtable, prim_types->audio_type.id)->llvm_type = audio_type_ref;

    // TODO: Finish
    // ()
    LLVMTypeRef unit_type_ref = LLVMStructCreateNamed(codegen->context, "_Unit");
    LLVMTypeRef unit_elems[1] = { necro_type_ref };
    LLVMStructSetBody(unit_type_ref, unit_elems, 1, false);
    necro_symtable_get(codegen->symtable, prim_types->unit_type.id)->llvm_type = unit_type_ref;

    // []
    LLVMTypeRef list_type_ref = LLVMStructCreateNamed(codegen->context, "_List");
    LLVMTypeRef list_elems[3] = { necro_type_ref, LLVMPointerType(necro_val_type_ref, 0), LLVMPointerType(necro_val_type_ref, 0) };
    LLVMStructSetBody(list_type_ref, list_elems, 3, false);
    necro_symtable_get(codegen->symtable, prim_types->list_type.id)->llvm_type = list_type_ref;

    // TODO: Need to solve polymorphism problem!
    {
        // delay
        LLVMTypeRef delay_type_ref = LLVMStructCreateNamed(codegen->context, "_DelayNode");
        LLVMTypeRef delay_elems[3] = { necro_type_ref, LLVMPointerType(necro_val_type_ref, 0), LLVMPointerType(necro_val_type_ref, 0) };
        LLVMStructSetBody(delay_type_ref, delay_elems, 3, false); // NOTE: Switching this to have no buffer!
        NecroNodePrototype* delay_node = necro_create_necro_node_prototype(codegen, necro_con_to_var(codegen->infer->prim_types->delay_fn), "_DelayNode", delay_type_ref, LLVMPointerType(necro_val_type_ref, 0), NULL, NECRO_NODE_STATEFUL);
        necro_symtable_get(codegen->symtable, prim_types->delay_fn.id)->llvm_type      = delay_type_ref; // Is this correct!?
        necro_symtable_get(codegen->symtable, prim_types->delay_fn.id)->node_prototype = delay_node;
        delay_node->args = necro_cons_arg_list(&codegen->arena, (NecroArg) { .llvm_type = LLVMPointerType(necro_val_type_ref, 0), .var = { 0 } }, delay_node->args);
        delay_node->args = necro_cons_arg_list(&codegen->arena, (NecroArg) { .llvm_type = LLVMPointerType(necro_val_type_ref, 0), .var = { 0 } }, delay_node->args);
        {
            // _mkDelayNode
            LLVMValueRef       mk_delay_node = necro_snapshot_add_function(codegen, "_mkDelayNode", LLVMPointerType(delay_node->node_type, 0), NULL, 0);
            delay_node->mk_function          = mk_delay_node;
            LLVMBasicBlockRef  entry         = LLVMAppendBasicBlock(mk_delay_node, "entry");
            LLVMPositionBuilderAtEnd(codegen->builder, entry);
            LLVMValueRef       void_ptr      = necro_alloc_codegen(codegen, delay_node->node_type, 2);
            LLVMValueRef       node_ptr      = LLVMBuildBitCast(codegen->builder, void_ptr, LLVMPointerType(delay_node->node_type, 0), "node_ptr");
            necro_init_necro_data(codegen, node_ptr, 0, 0);
            LLVMValueRef       val_ptr       = necro_snapshot_gep(codegen, "val_ptr", node_ptr, 2, (uint32_t[]) { 0, 1 });
            LLVMBuildStore(codegen->builder, LLVMConstPointerNull(LLVMPointerType(necro_val_type_ref, 0)), val_ptr);
            LLVMValueRef       buf_ptr       = necro_snapshot_gep(codegen, "buf_ptr", node_ptr, 2, (uint32_t[]) { 0, 2 });
            LLVMBuildStore(codegen->builder, LLVMConstPointerNull(LLVMPointerType(necro_val_type_ref, 0)), val_ptr);
            LLVMBuildRet(codegen->builder, node_ptr);
        }
        {
            // _initDelayNode
            LLVMValueRef       init_delay_node = necro_snapshot_add_function(codegen, "_initDelayNode", LLVMVoidTypeInContext(codegen->context), (LLVMTypeRef[]) { LLVMPointerType(delay_node->node_type, 0) }, 1);
            delay_node->init_function          = init_delay_node;
            LLVMBasicBlockRef  entry           = LLVMAppendBasicBlock(init_delay_node, "entry");
            LLVMPositionBuilderAtEnd(codegen->builder, entry);
            LLVMBuildRetVoid(codegen->builder);
        }
        {
            // _callDelayNode
            LLVMValueRef       call_delay_Node = necro_snapshot_add_function(codegen, "_callDelayNode", LLVMPointerType(necro_val_type_ref, 0), (LLVMTypeRef[]) { LLVMPointerType(delay_node->node_type, 0), LLVMPointerType(necro_val_type_ref, 0), LLVMPointerType(necro_val_type_ref, 0) }, 3);
            delay_node->call_function          = call_delay_Node;
            LLVMBasicBlockRef  entry           = LLVMAppendBasicBlock(call_delay_Node, "entry");
            LLVMPositionBuilderAtEnd(codegen->builder, entry);
            LLVMValueRef       node_ptr        = LLVMGetParam(call_delay_Node, 0);
            // LLVMValueRef       val_ptr         = necro_snapshot_gep(codegen, "val_ptr", node_ptr, 2, (uint32_t[]) { 0, 1 });
            // LLVMValueRef       val_val         = LLVMBuildLoad(codegen->builder, val_ptr, "val_val");
            LLVMValueRef       buf_ptr         = necro_snapshot_gep(codegen, "buf_ptr", node_ptr, 2, (uint32_t[]) { 0, 2 });
            LLVMValueRef       buf_val         = LLVMBuildLoad(codegen->builder, buf_ptr, "buf_val");
            LLVMValueRef       tag_ptr         = necro_snapshot_gep(codegen, "tag_ptr", node_ptr, 3, (uint32_t[]) { 0, 0, 1 });
            LLVMValueRef       tag_value       = LLVMBuildLoad(codegen->builder, tag_ptr, "tag_value");
            LLVMValueRef       needs_init      = LLVMBuildICmp(codegen->builder, LLVMIntEQ, LLVMConstInt(LLVMInt32TypeInContext(codegen->context), 0, false), tag_value, "needs_init");
            LLVMBasicBlockRef  init_block      = LLVMAppendBasicBlock(call_delay_Node, "init_block");
            LLVMBasicBlockRef  cont_block      = LLVMAppendBasicBlock(call_delay_Node, "cont_block");
            LLVMBasicBlockRef  null_block      = LLVMAppendBasicBlock(call_delay_Node, "null_block");
            LLVMBasicBlockRef  val_block       = LLVMAppendBasicBlock(call_delay_Node, "val_block");
            LLVMBuildCondBr(codegen->builder, needs_init, init_block, cont_block);
            LLVMPositionBuilderAtEnd(codegen->builder, init_block);
            LLVMBuildStore(codegen->builder, LLVMGetParam(call_delay_Node, 2), buf_ptr);
            // LLVMBuildStore(codegen->builder, LLVMGetParam(call_delay_Node, 1), val_ptr);
            LLVMBuildStore(codegen->builder, LLVMConstInt(LLVMInt32TypeInContext(codegen->context), 1, false), tag_ptr);
            LLVMBuildRet(codegen->builder, LLVMGetParam(call_delay_Node, 1));
            LLVMPositionBuilderAtEnd(codegen->builder, cont_block);
            LLVMValueRef       is_null         = LLVMBuildICmp(codegen->builder, LLVMIntEQ, buf_val, LLVMConstPointerNull(LLVMPointerType(necro_val_type_ref, 0)), "is_null");
            LLVMBuildCondBr(codegen->builder, is_null, null_block, val_block);
            LLVMPositionBuilderAtEnd(codegen->builder, null_block);
            // LLVMBuildStore(codegen->builder, LLVMGetParam(call_delay_Node, 2), val_ptr);
            LLVMBuildRet(codegen->builder, LLVMGetParam(call_delay_Node, 2));
            LLVMPositionBuilderAtEnd(codegen->builder, val_block);
            LLVMBuildStore(codegen->builder, LLVMGetParam(call_delay_Node, 2), buf_ptr);
            // LLVMBuildStore(codegen->builder, buf_val, val_ptr);
            LLVMBuildRet(codegen->builder, buf_val);
            // // LLVMBuildStore(codegen->builder, LLVMConstPointerNull(LLVMPointerType(necro_val_type_ref, 0)), val_ptr);
        }
    }

    // Sequence
    // NecroASTNode* sequence_s_type       = necro_create_simple_type_ast(&prim_types->arena, intern, "Sequence", necro_create_var_list_ast(&prim_types->arena, intern, 1, NECRO_VAR_TYPE_VAR_DECLARATION));

    // IO
    // NecroASTNode* io_s_type   = necro_create_simple_type_ast(&prim_types->arena, intern, "IO", necro_create_var_list_ast(&prim_types->arena, intern, 1, NECRO_VAR_TYPE_VAR_DECLARATION));

    // Event
    // NecroASTNode* event_s_type   = necro_create_simple_type_ast(&prim_types->arena, intern, "Event", necro_create_var_list_ast(&prim_types->arena, intern, 1, NECRO_VAR_TYPE_VAR_DECLARATION));

    // Pattern
    // NecroASTNode* pattern_s_type   = necro_create_simple_type_ast(&prim_types->arena, intern, "Pattern", necro_create_var_list_ast(&prim_types->arena, intern, 1, NECRO_VAR_TYPE_VAR_DECLARATION));

    // Bool
    // NecroASTNode* bool_s_type           = necro_create_simple_type_ast(&prim_types->arena, intern, "Bool", NULL);

    // CodeGen for type classes!!!!

    if (necro_verify_and_dump_codegen(codegen) == NECRO_ERROR)
        return NECRO_ERROR;

    // // Clean up and return
    // codegen->mod     = codegen_mod;
    // codegen->builder = codegen_builder;
    return NECRO_SUCCESS;
}

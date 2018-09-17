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
#include "symtable.h"

//=====================================================
// Forward Declarations
//=====================================================
void necro_prim_types_init_prim_defs(NecroPrimTypes* prim_types, NecroIntern* intern);

//=====================================================
// Create / Destroy
//=====================================================
NecroPrimTypes necro_prim_types_empty()
{
    return (NecroPrimTypes)
    {
        .arena     = necro_paged_arena_empty(),
        .defs      = NULL,
        .def_head  = NULL,
        .con_table = necro_empty_con_table(),
    };
}

NecroPrimTypes necro_prim_types_create()
{
    return (NecroPrimTypes)
    {
        .arena     = necro_paged_arena_create(),
        .defs      = NULL,
        .def_head  = NULL,
        .con_table = necro_create_con_table()
    };
}

void necro_prim_types_destroy(NecroPrimTypes* prim_types)
{
    necro_paged_arena_destroy(&prim_types->arena);
    necro_destroy_con_table(&prim_types->con_table);
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

NecroPrimDef* necro_prim_def_data(NecroPrimTypes* prim_types, NecroIntern* intern, NecroCon* global_name, NecroAst* data_declaration_ast)
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

NecroPrimDef* necro_prim_def_class(NecroPrimTypes* prim_types, NecroIntern* intern, NecroCon* global_name, NecroAst* type_class_ast)
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

NecroPrimDef* necro_prim_def_instance(NecroPrimTypes* prim_types, NecroIntern* intern, NecroAst* instance_ast)
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

NecroPrimDef* necro_prim_def_fun(NecroPrimTypes* prim_types, NecroIntern* intern, NecroCon* global_name, NecroAst* type_sig_ast, size_t arity)
{
    UNUSED(intern);
    assert(type_sig_ast->type == NECRO_AST_TYPE_SIGNATURE);
    NecroSymbol     fun_symbol      = type_sig_ast->type_signature.var->variable.symbol;
    NecroPrimDef*   fun_def         = necro_create_prim_def(prim_types, NECRO_PRIM_DEF_FUN, global_name);
    NecroCon        fun_name        = (NecroCon) { .id = { 0 }, .symbol = fun_symbol };
    if (global_name != NULL)
        *global_name = fun_name;
    fun_def->name                 = fun_name;
    fun_def->fun_def.type         = NULL;
    fun_def->fun_def.type_sig_ast = type_sig_ast;
    fun_def->fun_def.arity        = arity;
    return fun_def;
}

NecroPrimDef* necro_prim_def_bin_op(NecroPrimTypes* prim_types, NecroIntern* intern, NecroCon* global_name, NecroAst* type_sig_ast, NecroAst* definition_ast)
{
    UNUSED(intern);
    assert(type_sig_ast != NULL);
    assert(type_sig_ast->type == NECRO_AST_TYPE_SIGNATURE);
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

NECRO_RETURN_CODE necro_prim_types_build_scopes(NecroPrimTypes* prim_types, NecroScopedSymTable* scoped_symtable)
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

NECRO_RETURN_CODE necro_prim_types_rename(NecroPrimTypes* prim_types, NecroRenamer* renamer)
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
            if (renamer->error.return_code != NECRO_SUCCESS) return renamer->error.return_code;
            necro_rename_var_pass(renamer, &prim_types->arena, def->data_def.data_declaration_ast);
            if (renamer->error.return_code != NECRO_SUCCESS) return renamer->error.return_code;
            // add constructors to con table
            NecroAst* constructor_list = def->data_def.data_declaration_ast->data_declaration.constructor_list;
            while (constructor_list != NULL)
            {
                NecroCon con = (NecroCon) { .id = constructor_list->list.item->constructor.conid->conid.id, .symbol = constructor_list->list.item->constructor.conid->conid.symbol };
                necro_con_table_insert(&prim_types->con_table, con.symbol->symbol_num, &con);
                constructor_list = constructor_list->list.next_item;
            }
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
            if (renamer->error.return_code != NECRO_SUCCESS) return renamer->error.return_code;
            necro_rename_var_pass(renamer, &prim_types->arena, def->class_def.type_class_ast);
            if (renamer->error.return_code != NECRO_SUCCESS) return renamer->error.return_code;
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
            if (renamer->error.return_code != NECRO_SUCCESS) return renamer->error.return_code;
            necro_rename_var_pass(renamer, &prim_types->arena, def->fun_def.type_sig_ast);
            if (renamer->error.return_code != NECRO_SUCCESS) return renamer->error.return_code;
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
            if (renamer->error.return_code != NECRO_SUCCESS) return renamer->error.return_code;
            necro_rename_declare_pass(renamer, &prim_types->arena, def->bin_op_def.definition_ast);
            if (renamer->error.return_code != NECRO_SUCCESS) return renamer->error.return_code;
            necro_rename_var_pass(renamer, &prim_types->arena, def->bin_op_def.type_sig_ast);
            if (renamer->error.return_code != NECRO_SUCCESS) return renamer->error.return_code;
            necro_rename_var_pass(renamer, &prim_types->arena, def->bin_op_def.definition_ast);
            if (renamer->error.return_code != NECRO_SUCCESS) return renamer->error.return_code;
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
            if (renamer->error.return_code != NECRO_SUCCESS) { necro_ast_print(def->instance_def.instance_ast); return renamer->error.return_code; }
            necro_rename_var_pass(renamer, &prim_types->arena, def->instance_def.instance_ast);
            if (renamer->error.return_code != NECRO_SUCCESS) return renamer->error.return_code;
            // necro_print_reified_ast_node(def->instance_def.instance_ast, renamer->intern);
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

NECRO_RETURN_CODE necro_prim_types_infer(NecroPrimTypes* prim_types, NecroDependencyAnalyzer* d_analyzer, NecroInfer* infer, NECRO_PHASE phase)
{
    NecroPrimDef* def  = prim_types->def_head;
    NecroAst* top  = NULL;
    NecroAst* head = NULL;
    // Turn into single top_level declaration
    while (def != NULL)
    {
        switch (def->type)
        {
        case NECRO_PRIM_DEF_DATA:
            if (top == NULL)
            {
                top  = necro_ast_create_top_decl(&prim_types->arena, def->data_def.data_declaration_ast, NULL);
                head = top;
            }
            else
            {
                top->top_declaration.next_top_decl = necro_ast_create_top_decl(&prim_types->arena, def->data_def.data_declaration_ast, NULL);
                top                                = top->top_declaration.next_top_decl;
            }
            break;
        case NECRO_PRIM_DEF_CLASS:
            if (top == NULL)
            {
                top  = necro_ast_create_top_decl(&prim_types->arena, def->class_def.type_class_ast, NULL);
                head = top;
            }
            else
            {
                top->top_declaration.next_top_decl = necro_ast_create_top_decl(&prim_types->arena, def->class_def.type_class_ast, NULL);
                top                                = top->top_declaration.next_top_decl;
            }
            break;
        case NECRO_PRIM_DEF_INSTANCE:
            if (top == NULL)
            {
                top  = necro_ast_create_top_decl(&prim_types->arena, def->instance_def.instance_ast, NULL);
                head = top;
            }
            else
            {
                top->top_declaration.next_top_decl = necro_ast_create_top_decl(&prim_types->arena, def->instance_def.instance_ast, NULL);
                top                                = top->top_declaration.next_top_decl;
            }
            break;
        case NECRO_PRIM_DEF_FUN:
            assert(def->fun_def.arity <= INT32_MAX);
            necro_symtable_get(infer->symtable, def->fun_def.type_sig_ast->type_signature.var->variable.id)->arity = (int32_t) def->fun_def.arity;
            if (top == NULL)
            {
                top  = necro_ast_create_top_decl(&prim_types->arena, def->fun_def.type_sig_ast, NULL);
                head = top;
            }
            else
            {
                top->top_declaration.next_top_decl = necro_ast_create_top_decl(&prim_types->arena, def->fun_def.type_sig_ast, NULL);
                top                                = top->top_declaration.next_top_decl;
            }
            break;
        case NECRO_PRIM_DEF_BIN_OP:
            necro_symtable_get(infer->symtable, def->bin_op_def.type_sig_ast->type_signature.var->variable.id)->arity = 2;
            if (top == NULL)
            {
                top  = necro_ast_create_top_decl(&prim_types->arena, def->bin_op_def.type_sig_ast, necro_ast_create_top_decl(&prim_types->arena, def->bin_op_def.definition_ast, NULL));
                head = top;
            }
            else
            {
                top->top_declaration.next_top_decl = necro_ast_create_top_decl(&prim_types->arena, def->bin_op_def.type_sig_ast, necro_ast_create_top_decl(&prim_types->arena, def->bin_op_def.definition_ast, NULL));
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
        necro_ast_print(head);
    }

    return infer->error.return_code;
}

// TODO: Finish
void necro_derive_eq(NecroPrimTypes* prim_types, NecroIntern* intern, NecroPrimDef* dat_def)
{
    UNUSED(prim_types);
    UNUSED(intern);
    UNUSED(dat_def);
}

// TODO: Save % for rational types
void necro_create_prim_num(NecroPrimTypes* prim_types, NecroIntern* intern, NecroCon* global_name, const char* data_type_name)
{
    NecroAst* s_type   = necro_ast_create_simple_type(&prim_types->arena, intern, data_type_name, NULL);
    NecroAst* n_con    = necro_ast_create_data_con(&prim_types->arena, intern, data_type_name, NULL);
    NecroAst* con_list = necro_ast_create_list(&prim_types->arena, n_con, NULL);
    NecroPrimDef* data_def = necro_prim_def_data(prim_types, intern, global_name, necro_ast_create_data_declaration(&prim_types->arena, intern, s_type, con_list));
    UNUSED(data_def);
}

NecroAst* necro_make_poly_con1(NecroPagedArena* arena, NecroIntern* intern, const char* data_type_name)
{
    NecroAst* type_var_ast = necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
    NecroAst* type_constr  = necro_ast_create_data_con(arena, intern, data_type_name, necro_ast_create_list(arena, type_var_ast, NULL));
    type_constr->constructor.conid->conid.con_type = NECRO_CON_TYPE_VAR;
    // type_constr->conid.con_type = NECRO_CON_TYPE_VAR;
    return type_constr;
}

NecroAst* necro_make_num_context(NecroPagedArena* arena, NecroIntern* intern, const char* class_name, bool is_polymorphic)
{
    if (is_polymorphic)
        return necro_ast_create_context(arena, intern, class_name, "a", NULL);
    else
        return NULL;
}

NecroAst* necro_make_num_con(NecroPagedArena* arena, NecroIntern* intern, const char* data_type_name, bool is_polymorphic)
{
    if (is_polymorphic)
        return necro_make_poly_con1(arena, intern, data_type_name);
    else
        return necro_ast_create_conid(arena, intern, data_type_name, NECRO_CON_TYPE_VAR);
}

NecroAst* necro_make_bin_math_ast(NecroPrimTypes* prim_types, NecroIntern* intern, const char* data_type_name, bool is_poly)
{
    return necro_ast_create_type_fn(&prim_types->arena,
        necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly),
        necro_ast_create_type_fn(&prim_types->arena,
            necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly),
            necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly)));
}

NecroAst* necro_make_unary_math_ast(NecroPrimTypes* prim_types, NecroIntern* intern, const char* data_type_name, bool is_poly)
{
    return necro_ast_create_type_fn(&prim_types->arena,
        necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly),
        necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly));
}

// TODO: Deriving mechanism
void necro_create_prim_num_instances(NecroPrimTypes* prim_types, NecroIntern* intern, const char* data_type_name, bool is_poly, bool is_fractional)
{
    NecroSymbol   data_name  = necro_intern_string(intern, data_type_name);
    NecroAst* bool_conid = necro_ast_create_conid(&prim_types->arena, intern, "Bool", NECRO_CON_TYPE_VAR);

    //--------------
    // Num

    const char*   add_symbol        = necro_intern_concat_symbols(intern, necro_intern_string(intern, "primAdd@"), data_name)->str;
    NecroPrimDef* add_type_def      = necro_prim_def_fun(prim_types, intern, NULL, necro_ast_create_fn_type_sig(&prim_types->arena, intern, add_symbol, NULL, necro_make_bin_math_ast(prim_types, intern, data_type_name, is_poly), NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION), 2);
    UNUSED(add_type_def);

    const char*   sub_symbol        = necro_intern_concat_symbols(intern, necro_intern_string(intern, "primSub@"), data_name)->str;
    NecroPrimDef* sub_type_def      = necro_prim_def_fun(prim_types, intern, NULL, necro_ast_create_fn_type_sig(&prim_types->arena, intern, sub_symbol, NULL, necro_make_bin_math_ast(prim_types, intern, data_type_name, is_poly), NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION), 2);
    UNUSED(sub_type_def);

    const char*   mul_symbol        = necro_intern_concat_symbols(intern, necro_intern_string(intern, "primMul@"), data_name)->str;
    NecroPrimDef* mul_type_def      = necro_prim_def_fun(prim_types, intern, NULL, necro_ast_create_fn_type_sig(&prim_types->arena, intern, mul_symbol, NULL, necro_make_bin_math_ast(prim_types, intern, data_type_name, is_poly), NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION), 2);
    UNUSED(mul_type_def);

    const char*   abs_symbol        = necro_intern_concat_symbols(intern, necro_intern_string(intern, "primAbs@"), data_name)->str;
    NecroPrimDef* abs_type_def      = necro_prim_def_fun(prim_types, intern, NULL, necro_ast_create_fn_type_sig(&prim_types->arena, intern, abs_symbol, NULL, necro_make_unary_math_ast(prim_types, intern, data_type_name, is_poly), NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION), 2);
    UNUSED(abs_type_def);

    const char*   signum_symbol     = necro_intern_concat_symbols(intern, necro_intern_string(intern, "primSignum@"), data_name)->str;
    NecroPrimDef* signum_type_def   = necro_prim_def_fun(prim_types, intern, NULL, necro_ast_create_fn_type_sig(&prim_types->arena, intern, signum_symbol, NULL, necro_make_unary_math_ast(prim_types, intern, data_type_name, is_poly), NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION), 2);
    UNUSED(signum_type_def);

    const char*   from_int_symbol   = necro_intern_concat_symbols(intern, necro_intern_string(intern, "primFromInt@"), data_name)->str;
    NecroAst* from_int_ast      = necro_ast_create_type_fn(&prim_types->arena, necro_ast_create_conid(&prim_types->arena, intern, "Int", NECRO_CON_TYPE_VAR), necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly));
    NecroPrimDef* from_int_type_def = necro_prim_def_fun(prim_types, intern, NULL, necro_ast_create_fn_type_sig(&prim_types->arena, intern, from_int_symbol, NULL, from_int_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION), 1);
    UNUSED(from_int_type_def);

    NecroAst* num_method_list  =
        necro_ast_create_decl(&prim_types->arena, necro_ast_create_simple_assignment(&prim_types->arena, intern, "add", necro_ast_create_var(&prim_types->arena, intern, add_symbol, NECRO_VAR_VAR)),
            necro_ast_create_decl(&prim_types->arena, necro_ast_create_simple_assignment(&prim_types->arena, intern, "sub", necro_ast_create_var(&prim_types->arena, intern, sub_symbol, NECRO_VAR_VAR)),
                necro_ast_create_decl(&prim_types->arena, necro_ast_create_simple_assignment(&prim_types->arena, intern, "mul", necro_ast_create_var(&prim_types->arena, intern, mul_symbol, NECRO_VAR_VAR)),
                    necro_ast_create_decl(&prim_types->arena, necro_ast_create_simple_assignment(&prim_types->arena, intern, "abs", necro_ast_create_var(&prim_types->arena, intern, abs_symbol, NECRO_VAR_VAR)),
                        necro_ast_create_decl(&prim_types->arena, necro_ast_create_simple_assignment(&prim_types->arena, intern, "signum", necro_ast_create_var(&prim_types->arena, intern, signum_symbol, NECRO_VAR_VAR)),
                            necro_ast_create_decl(&prim_types->arena, necro_ast_create_simple_assignment(&prim_types->arena, intern, "fromInt", necro_ast_create_var(&prim_types->arena, intern, from_int_symbol, NECRO_VAR_VAR)), NULL))))));
    NecroAst* num_instance_ast = necro_ast_create_instance(&prim_types->arena, intern, "Num", necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly), necro_make_num_context(&prim_types->arena, intern, "Num", is_poly), num_method_list);
    NecroPrimDef* num_instance_def = necro_prim_def_instance(prim_types, intern, num_instance_ast);
    UNUSED(num_instance_def);

    //--------------
    // Fractional
    if (is_fractional)
    {
        const char*   div_symbol     = necro_intern_concat_symbols(intern, necro_intern_string(intern, "primDiv@"), data_name)->str;
        NecroPrimDef* div_type_def   = necro_prim_def_fun(prim_types, intern, NULL, necro_ast_create_fn_type_sig(&prim_types->arena, intern, div_symbol, NULL, necro_make_bin_math_ast(prim_types, intern, data_type_name, is_poly), NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION), 2);
        UNUSED(div_type_def);

        const char*   recip_symbol   = necro_intern_concat_symbols(intern, necro_intern_string(intern, "primRecip@"), data_name)->str;
        NecroPrimDef* recip_type_def = necro_prim_def_fun(prim_types, intern, NULL, necro_ast_create_fn_type_sig(&prim_types->arena, intern, recip_symbol, NULL, necro_make_unary_math_ast(prim_types, intern, data_type_name, is_poly), NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION), 2);
        UNUSED(recip_type_def);

        const char*   from_rational_symbol   = necro_intern_concat_symbols(intern, necro_intern_string(intern, "primFromRational@"), data_name)->str;
        NecroAst* from_rational_ast      = necro_ast_create_type_fn(&prim_types->arena, necro_ast_create_conid(&prim_types->arena, intern, "Rational", NECRO_CON_TYPE_VAR), necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly));
        NecroPrimDef* from_rational_type_def = necro_prim_def_fun(prim_types, intern, NULL, necro_ast_create_fn_type_sig(&prim_types->arena, intern, from_rational_symbol, NULL, from_rational_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION), 1);
        UNUSED(from_rational_type_def);

        NecroAst* fractional_method_list =
            necro_ast_create_decl(&prim_types->arena, necro_ast_create_simple_assignment(&prim_types->arena, intern, "div", necro_ast_create_var(&prim_types->arena, intern, div_symbol, NECRO_VAR_VAR)),
                necro_ast_create_decl(&prim_types->arena, necro_ast_create_simple_assignment(&prim_types->arena, intern, "recip", necro_ast_create_var(&prim_types->arena, intern, recip_symbol, NECRO_VAR_VAR)),
                    necro_ast_create_decl(&prim_types->arena, necro_ast_create_simple_assignment(&prim_types->arena, intern, "fromRational", necro_ast_create_var(&prim_types->arena, intern, from_rational_symbol, NECRO_VAR_VAR)), NULL)));
        NecroAst* fractional_instance_ast = necro_ast_create_instance(&prim_types->arena, intern, "Fractional", necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly), necro_make_num_context(&prim_types->arena, intern, "Fractional", is_poly), fractional_method_list);
        NecroPrimDef* fractional_instance_def = necro_prim_def_instance(prim_types, intern, fractional_instance_ast);
        UNUSED(fractional_instance_def);
    }

    //--------------
    // Eq
    NecroAst* bin_comp_ast = necro_ast_create_type_fn(&prim_types->arena, necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly), necro_ast_create_type_fn(&prim_types->arena, necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly), bool_conid));
    const char*   eq_symbol    = necro_intern_concat_symbols(intern, necro_intern_string(intern, "primEq@"), data_name)->str;
    NecroPrimDef* eq_type_def  = necro_prim_def_fun(prim_types, intern, NULL, necro_ast_create_fn_type_sig(&prim_types->arena, intern, eq_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION), 2);
    UNUSED(eq_type_def);

    const char*   neq_symbol   = necro_intern_concat_symbols(intern, necro_intern_string(intern, "primNEq@"), data_name)->str;
    NecroPrimDef* neq_type_def = necro_prim_def_fun(prim_types, intern, NULL, necro_ast_create_fn_type_sig(&prim_types->arena, intern, neq_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION), 2);
    UNUSED(neq_type_def);

    NecroAst* eq_method_list =
        necro_ast_create_decl(&prim_types->arena, necro_ast_create_simple_assignment(&prim_types->arena, intern, "eq", necro_ast_create_var(&prim_types->arena, intern, eq_symbol, NECRO_VAR_VAR)),
            necro_ast_create_decl(&prim_types->arena, necro_ast_create_simple_assignment(&prim_types->arena, intern, "neq", necro_ast_create_var(&prim_types->arena, intern, neq_symbol, NECRO_VAR_VAR)), NULL));

    NecroAst* eq_instance_ast = necro_ast_create_instance(&prim_types->arena, intern, "Eq", necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly), necro_make_num_context(&prim_types->arena, intern, "Eq", is_poly), eq_method_list);
    NecroPrimDef* eq_instance_def = necro_prim_def_instance(prim_types, intern, eq_instance_ast);
    UNUSED(eq_instance_def);

    //--------------
    // Ord
    const char*   gt_symbol    = necro_intern_concat_symbols(intern, necro_intern_string(intern, "primGT@"), data_name)->str;
    NecroPrimDef* gt_type_def  = necro_prim_def_fun(prim_types, intern, NULL, necro_ast_create_fn_type_sig(&prim_types->arena, intern, gt_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION), 2);
    UNUSED(gt_type_def);

    const char*   lt_symbol    = necro_intern_concat_symbols(intern, necro_intern_string(intern, "primLT@"), data_name)->str;
    NecroPrimDef* lt_type_def  = necro_prim_def_fun(prim_types, intern, NULL, necro_ast_create_fn_type_sig(&prim_types->arena, intern, lt_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION), 2);
    UNUSED(lt_type_def);

    const char*   gte_symbol   = necro_intern_concat_symbols(intern, necro_intern_string(intern, "primGTE@"), data_name)->str;
    NecroPrimDef* gte_type_def = necro_prim_def_fun(prim_types, intern, NULL, necro_ast_create_fn_type_sig(&prim_types->arena, intern, gte_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION), 2);
    UNUSED(gte_type_def);

    const char*   lte_symbol   = necro_intern_concat_symbols(intern, necro_intern_string(intern, "primLTE@"), data_name)->str;
    NecroPrimDef* lte_type_def = necro_prim_def_fun(prim_types, intern, NULL, necro_ast_create_fn_type_sig(&prim_types->arena, intern, lte_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION), 2);
    UNUSED(lte_type_def);

    NecroAst* ord_method_list =
        necro_ast_create_decl(&prim_types->arena, necro_ast_create_simple_assignment(&prim_types->arena, intern, "lt", necro_ast_create_var(&prim_types->arena, intern, lt_symbol, NECRO_VAR_VAR)),
            necro_ast_create_decl(&prim_types->arena, necro_ast_create_simple_assignment(&prim_types->arena, intern, "gt", necro_ast_create_var(&prim_types->arena, intern, gt_symbol, NECRO_VAR_VAR)),
                necro_ast_create_decl(&prim_types->arena, necro_ast_create_simple_assignment(&prim_types->arena, intern, "lte", necro_ast_create_var(&prim_types->arena, intern, lte_symbol, NECRO_VAR_VAR)),
                    necro_ast_create_decl(&prim_types->arena, necro_ast_create_simple_assignment(&prim_types->arena, intern, "gte", necro_ast_create_var(&prim_types->arena, intern, gte_symbol, NECRO_VAR_VAR)), NULL))));
    NecroAst* ord_instance_ast = necro_ast_create_instance(&prim_types->arena, intern, "Ord", necro_make_num_con(&prim_types->arena, intern, data_type_name, is_poly), necro_make_num_context(&prim_types->arena, intern, "Ord", is_poly), ord_method_list);
    NecroPrimDef* ord_instance_def = necro_prim_def_instance(prim_types, intern, ord_instance_ast);
    UNUSED(ord_instance_def);
}

NecroAst* necro_create_class_comp_sig(NecroPagedArena* arena, NecroIntern* intern, const char* sig_name)
{
    return
        necro_ast_create_fn_type_sig(arena, intern, sig_name, NULL,
            necro_ast_create_type_fn(arena,
                necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                necro_ast_create_type_fn(arena,
                    necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                    necro_ast_create_conid(arena, intern, "Bool", NECRO_CON_TYPE_VAR))), NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
}

NecroAst* necro_create_class_bin_op_sig(NecroPagedArena* arena, NecroIntern* intern, const char* sig_name)
{
    return
        necro_ast_create_fn_type_sig(arena, intern, sig_name, NULL,
            necro_ast_create_type_fn(arena,
                necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                necro_ast_create_type_fn(arena,
                    necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                    necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR))), NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
}

NecroAst* necro_create_class_unary_op_sig(NecroPagedArena* arena, NecroIntern* intern, const char* sig_name)
{
    return
        necro_ast_create_fn_type_sig(arena, intern, sig_name, NULL,
            necro_ast_create_type_fn(arena,
                necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR)), NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
}

NecroAst* necro_create_num_bin_op_sig(NecroPagedArena* arena, NecroIntern* intern, const char* sig_name)
{
    return
        necro_ast_create_fn_type_sig(arena, intern, sig_name, necro_ast_create_context(arena, intern, "Num", "a", NULL),
            necro_ast_create_type_fn(arena,
                necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                necro_ast_create_type_fn(arena,
                    necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                    necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR))), NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
}

NecroAst* necro_create_fractional_bin_op_sig(NecroPagedArena* arena, NecroIntern* intern, const char* sig_name)
{
    return
        necro_ast_create_fn_type_sig(arena, intern, sig_name, necro_ast_create_context(arena, intern, "Fractional", "a", NULL),
            necro_ast_create_type_fn(arena,
                necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                necro_ast_create_type_fn(arena,
                    necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                    necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR))), NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
}

NecroAst* necro_create_eq_comp_sig(NecroPagedArena* arena, NecroIntern* intern, const char* sig_name)
{
    return
        necro_ast_create_fn_type_sig(arena, intern, sig_name, necro_ast_create_context(arena, intern, "Eq", "a", NULL),
            necro_ast_create_type_fn(arena,
                necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                necro_ast_create_type_fn(arena,
                    necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                    necro_ast_create_conid(arena, intern, "Bool", NECRO_CON_TYPE_VAR))), NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
}

NecroAst* necro_create_ord_comp_sig(NecroPagedArena* arena, NecroIntern* intern, const char* sig_name)
{
    return
        necro_ast_create_fn_type_sig(arena, intern, sig_name, necro_ast_create_context(arena, intern, "Ord", "a", NULL),
            necro_ast_create_type_fn(arena,
                necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                necro_ast_create_type_fn(arena,
                    necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                    necro_ast_create_conid(arena, intern, "Bool", NECRO_CON_TYPE_VAR))), NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
}

void necro_bool_instances(NecroPrimTypes* prim_types, NecroIntern* intern)
{
    NecroSymbol   data_name  = necro_intern_string(intern, "Bool");
    NecroAst* bool_conid = necro_ast_create_conid(&prim_types->arena, intern, "Bool", NECRO_CON_TYPE_VAR);
    //--------------
    // Eq
    NecroAst* bin_comp_ast = necro_ast_create_type_fn(&prim_types->arena, bool_conid, necro_ast_create_type_fn(&prim_types->arena, bool_conid, bool_conid));
    const char*   eq_symbol    = necro_intern_concat_symbols(intern, necro_intern_string(intern, "primEq@"), data_name)->str;
    NecroPrimDef* eq_type_def  = necro_prim_def_fun(prim_types, intern, NULL, necro_ast_create_fn_type_sig(&prim_types->arena, intern, eq_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION), 2);
    UNUSED(eq_type_def);

    const char*   neq_symbol   = necro_intern_concat_symbols(intern, necro_intern_string(intern, "primNEq@"), data_name)->str;
    NecroPrimDef* neq_type_def = necro_prim_def_fun(prim_types, intern, NULL, necro_ast_create_fn_type_sig(&prim_types->arena, intern, neq_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION), 2);
    UNUSED(neq_type_def);

    NecroAst* eq_method_list =
        necro_ast_create_decl(&prim_types->arena, necro_ast_create_simple_assignment(&prim_types->arena, intern, "eq", necro_ast_create_var(&prim_types->arena, intern, eq_symbol, NECRO_VAR_VAR)),
            necro_ast_create_decl(&prim_types->arena, necro_ast_create_simple_assignment(&prim_types->arena, intern, "neq", necro_ast_create_var(&prim_types->arena, intern, neq_symbol, NECRO_VAR_VAR)), NULL));
    NecroAst* eq_instance_ast = necro_ast_create_instance(&prim_types->arena, intern, "Eq", bool_conid, NULL, eq_method_list);
    NecroPrimDef* eq_instance_def = necro_prim_def_instance(prim_types, intern, eq_instance_ast);
    UNUSED(eq_instance_def);

    //--------------
    // Ord
    const char*   gt_symbol    = necro_intern_concat_symbols(intern, necro_intern_string(intern, "primGT@"), data_name)->str;
    NecroPrimDef* gt_type_def  = necro_prim_def_fun(prim_types, intern, NULL, necro_ast_create_fn_type_sig(&prim_types->arena, intern, gt_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION), 2);
    UNUSED(gt_type_def);

    const char*   lt_symbol    = necro_intern_concat_symbols(intern, necro_intern_string(intern, "primLT@"), data_name)->str;
    NecroPrimDef* lt_type_def  = necro_prim_def_fun(prim_types, intern, NULL, necro_ast_create_fn_type_sig(&prim_types->arena, intern, lt_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION), 2);
    UNUSED(lt_type_def);

    const char*   gte_symbol   = necro_intern_concat_symbols(intern, necro_intern_string(intern, "primGTE@"), data_name)->str;
    NecroPrimDef* gte_type_def = necro_prim_def_fun(prim_types, intern, NULL, necro_ast_create_fn_type_sig(&prim_types->arena, intern, gte_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION), 2);
    UNUSED(gte_type_def);

    const char*   lte_symbol   = necro_intern_concat_symbols(intern, necro_intern_string(intern, "primLTE@"), data_name)->str;
    NecroPrimDef* lte_type_def = necro_prim_def_fun(prim_types, intern, NULL, necro_ast_create_fn_type_sig(&prim_types->arena, intern, lte_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION), 2);
    UNUSED(lte_type_def);

    NecroAst* ord_method_list =
        necro_ast_create_decl(&prim_types->arena, necro_ast_create_simple_assignment(&prim_types->arena, intern, "lt", necro_ast_create_var(&prim_types->arena, intern, lt_symbol, NECRO_VAR_VAR)),
            necro_ast_create_decl(&prim_types->arena, necro_ast_create_simple_assignment(&prim_types->arena, intern, "gt", necro_ast_create_var(&prim_types->arena, intern, gt_symbol, NECRO_VAR_VAR)),
                necro_ast_create_decl(&prim_types->arena, necro_ast_create_simple_assignment(&prim_types->arena, intern, "lte", necro_ast_create_var(&prim_types->arena, intern, lte_symbol, NECRO_VAR_VAR)),
                    necro_ast_create_decl(&prim_types->arena, necro_ast_create_simple_assignment(&prim_types->arena, intern, "gte", necro_ast_create_var(&prim_types->arena, intern, gte_symbol, NECRO_VAR_VAR)), NULL))));
    NecroAst* ord_instance_ast = necro_ast_create_instance(&prim_types->arena, intern, "Ord", bool_conid, NULL, ord_method_list);
    NecroPrimDef* ord_instance_def = necro_prim_def_instance(prim_types, intern, ord_instance_ast);
    UNUSED(ord_instance_def);
}

void necro_prim_types_init_prim_defs(NecroPrimTypes* prim_types, NecroIntern* intern)
{
    // _Poly
    NecroAst* any_s_type            = necro_ast_create_simple_type(&prim_types->arena, intern, "_Poly", NULL);
    NecroAst* any_null              = necro_ast_create_data_con(&prim_types->arena, intern, "_NullPoly", NULL);
    NecroAst* any_constructor_list  = necro_ast_create_list(&prim_types->arena, any_null, NULL);
    NecroPrimDef* any_data_def          = necro_prim_def_data(prim_types, intern, &prim_types->any_type, necro_ast_create_data_declaration(&prim_types->arena, intern, any_s_type, any_constructor_list));
    UNUSED(any_data_def);

    // ()
    NecroAst* unit_s_type           = necro_ast_create_simple_type(&prim_types->arena, intern, "()", NULL);
    NecroAst* unit_constructor      = necro_ast_create_data_con(&prim_types->arena, intern, "()", NULL);
    NecroAst* unit_constructor_list = necro_ast_create_list(&prim_types->arena, unit_constructor, NULL);
    NecroPrimDef* unit_data_def         = necro_prim_def_data(prim_types, intern, &prim_types->unit_type, necro_ast_create_data_declaration(&prim_types->arena, intern, unit_s_type, unit_constructor_list));
    UNUSED(unit_data_def);

    // []
    NecroAst* list_s_type           = necro_ast_create_simple_type(&prim_types->arena, intern, "[]", necro_ast_create_var_list(&prim_types->arena, intern, 1, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroAst* list_app              = necro_ast_create_type_app(&prim_types->arena, necro_ast_create_conid(&prim_types->arena, intern, "[]", NECRO_CON_TYPE_VAR), necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR));
    NecroAst* cons_args             = necro_ast_create_list(&prim_types->arena, necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR), necro_ast_create_list(&prim_types->arena, list_app, NULL));
    NecroAst* cons_constructor      = necro_ast_create_data_con(&prim_types->arena, intern, ":", cons_args);
    NecroAst* nil_constructor       = necro_ast_create_data_con(&prim_types->arena, intern, "[]", NULL);
    NecroAst* list_constructor_list = necro_ast_create_list(&prim_types->arena, cons_constructor, necro_ast_create_list(&prim_types->arena, nil_constructor, NULL));
    NecroPrimDef* list_data_def         = necro_prim_def_data(prim_types, intern, &prim_types->list_type, necro_ast_create_data_declaration(&prim_types->arena, intern, list_s_type, list_constructor_list));
    UNUSED(list_data_def);

    // Maybe
    NecroAst* maybe_s_type           = necro_ast_create_simple_type(&prim_types->arena, intern, "Maybe", necro_ast_create_var_list(&prim_types->arena, intern, 1, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroAst* maybe_app              = necro_ast_create_type_app(&prim_types->arena, necro_ast_create_conid(&prim_types->arena, intern, "Maybe", NECRO_CON_TYPE_VAR), necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR));
    UNUSED(maybe_app);
    NecroAst* just_constructor       = necro_ast_create_data_con(&prim_types->arena, intern, "Just", necro_ast_create_list(&prim_types->arena, necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR), NULL));
    NecroAst* nothing_constructor    = necro_ast_create_data_con(&prim_types->arena, intern, "Nothing", NULL);
    NecroAst* maybe_constructor_list = necro_ast_create_list(&prim_types->arena, just_constructor, necro_ast_create_list(&prim_types->arena, nothing_constructor, NULL));
    NecroPrimDef* maybe_data_def         = necro_prim_def_data(prim_types, intern, &prim_types->maybe_type, necro_ast_create_data_declaration(&prim_types->arena, intern, maybe_s_type, maybe_constructor_list));
    UNUSED(maybe_data_def);

    // World
    NecroAst* world_s_type   = necro_ast_create_simple_type(&prim_types->arena, intern, "World", NULL);
    NecroPrimDef* world_data_def = necro_prim_def_data(prim_types, intern, &prim_types->world_type, necro_ast_create_data_declaration(&prim_types->arena, intern, world_s_type, NULL));
    UNUSED(world_data_def);

    // Event
    NecroAst* event_s_type   = necro_ast_create_simple_type(&prim_types->arena, intern, "Event", necro_ast_create_var_list(&prim_types->arena, intern, 1, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroPrimDef* event_data_def = necro_prim_def_data(prim_types, intern, &prim_types->event_type, necro_ast_create_data_declaration(&prim_types->arena, intern, event_s_type, NULL));
    UNUSED(event_data_def);

    // Pattern
    NecroAst* pattern_s_type   = necro_ast_create_simple_type(&prim_types->arena, intern, "Pattern", necro_ast_create_var_list(&prim_types->arena, intern, 1, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroPrimDef* pattern_data_def = necro_prim_def_data(prim_types, intern, &prim_types->pattern_type, necro_ast_create_data_declaration(&prim_types->arena, intern, pattern_s_type, NULL));
    UNUSED(pattern_data_def);

    // Bool
    NecroAst* bool_s_type           = necro_ast_create_simple_type(&prim_types->arena, intern, "Bool", NULL);
    NecroAst* false_constructor     = necro_ast_create_data_con(&prim_types->arena, intern, "False", NULL);
    NecroAst* true_constructor      = necro_ast_create_data_con(&prim_types->arena, intern, "True", NULL);
    NecroAst* bool_constructor_list = necro_ast_create_list(&prim_types->arena, false_constructor, necro_ast_create_list(&prim_types->arena, true_constructor, NULL));
    NecroPrimDef* bool_data_def         = necro_prim_def_data(prim_types, intern, &prim_types->bool_type, necro_ast_create_data_declaration(&prim_types->arena, intern, bool_s_type, bool_constructor_list));
    UNUSED(bool_data_def);
    NecroAst* bool_type_conid       = necro_ast_create_conid(&prim_types->arena, intern, "Bool", NECRO_CON_TYPE_VAR);
    NecroAst* bool_comp_sig_ast     = necro_ast_create_fn_type_sig(&prim_types->arena, intern, "and", NULL, necro_ast_create_type_fn(&prim_types->arena, bool_type_conid, necro_ast_create_type_fn(&prim_types->arena, bool_type_conid, bool_type_conid)), NECRO_VAR_DECLARATION, NECRO_SIG_DECLARATION);
    UNUSED(bool_comp_sig_ast);

    // Prim Numbers
    necro_create_prim_num(prim_types, intern, &prim_types->int_type, "Int");
    necro_create_prim_num(prim_types, intern, &prim_types->float_type, "Float");
    necro_create_prim_num(prim_types, intern, &prim_types->audio_type, "Audio");
    necro_create_prim_num(prim_types, intern, &prim_types->rational_type, "Rational");
    necro_create_prim_num(prim_types, intern, &prim_types->char_type, "Char");

    // Ptr
    NecroAst* ptr_s_type   = necro_ast_create_simple_type(&prim_types->arena, intern, "Ptr", necro_ast_create_var_list(&prim_types->arena, intern, 1, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroPrimDef* ptr_data_def = necro_prim_def_data(prim_types, intern, &prim_types->ptr_type, necro_ast_create_data_declaration(&prim_types->arena, intern, ptr_s_type, NULL));
    UNUSED(ptr_data_def);

    // Array
    NecroAst* array_s_type   = necro_ast_create_simple_type(&prim_types->arena, intern, "Array", necro_ast_create_var_list(&prim_types->arena, intern, 1, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroAst* array_args     = necro_ast_create_list(&prim_types->arena,
        necro_ast_create_conid(&prim_types->arena, intern, "Int", NECRO_CON_TYPE_VAR),
            necro_ast_create_list(&prim_types->arena,
                necro_ast_create_type_app(&prim_types->arena,
                    necro_ast_create_conid(&prim_types->arena, intern, "Ptr", NECRO_CON_TYPE_VAR),
                    necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR)),
                NULL));
    NecroAst* array_con      = necro_ast_create_data_con(&prim_types->arena, intern, "Array", array_args);
    NecroAst* array_con_list = necro_ast_create_list(&prim_types->arena, array_con, NULL);
    NecroPrimDef* array_data_def = necro_prim_def_data(prim_types, intern, &prim_types->array_type, necro_ast_create_data_declaration(&prim_types->arena, intern, array_s_type, array_con_list));
    UNUSED(array_data_def);

    // _Closure
    NecroAst* closure_s_type   = necro_ast_create_simple_type(&prim_types->arena, intern, "_Closure", necro_ast_create_var_list(&prim_types->arena, intern, 1, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroAst* closure_args     =
        necro_ast_create_list(&prim_types->arena, necro_ast_create_conid(&prim_types->arena, intern, "Int", NECRO_CON_TYPE_VAR),
            necro_ast_create_list(&prim_types->arena, necro_ast_create_conid(&prim_types->arena, intern, "Int", NECRO_CON_TYPE_VAR),
                necro_ast_create_list(&prim_types->arena, necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR), NULL)));
    NecroAst* closure_con      = necro_ast_create_data_con(&prim_types->arena, intern, "_Closure", closure_args);
    NecroAst* closure_con_list = necro_ast_create_list(&prim_types->arena, closure_con, NULL);
    NecroPrimDef* closure_data_def = necro_prim_def_data(prim_types, intern, &prim_types->closure_type, necro_ast_create_data_declaration(&prim_types->arena, intern, closure_s_type, closure_con_list));
    UNUSED(closure_data_def);
    NecroAst* apply_type_ast  = necro_ast_create_type_fn(&prim_types->arena, necro_ast_create_type_app(&prim_types->arena, necro_ast_create_conid(&prim_types->arena, intern, "_Closure", NECRO_CON_TYPE_VAR), necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR)), necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR));
    NecroAst* apply_sig_ast   = necro_ast_create_fn_type_sig(&prim_types->arena, intern, "_apply", NULL, apply_type_ast, NECRO_VAR_DECLARATION, NECRO_SIG_DECLARATION);
    NecroPrimDef* apply_type_def  = necro_prim_def_fun(prim_types, intern, &prim_types->apply_fn, apply_sig_ast, 1);
    UNUSED(apply_type_def);

    // _DynState
    NecroAst* dyn_state_s_type           = necro_ast_create_simple_type(&prim_types->arena, intern, "_DynState", necro_ast_create_var_list(&prim_types->arena, intern, 1, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroAst* dyn_state_constructor      = necro_ast_create_data_con(&prim_types->arena, intern, "_DynState",
        necro_ast_create_list(&prim_types->arena, necro_ast_create_conid(&prim_types->arena, intern, "_Poly", NECRO_CON_TYPE_VAR),
            necro_ast_create_list(&prim_types->arena, necro_ast_create_conid(&prim_types->arena, intern, "Int", NECRO_CON_TYPE_VAR), NULL)));
    NecroAst* dyn_state_constructor_list = necro_ast_create_list(&prim_types->arena, dyn_state_constructor, NULL);
    NecroPrimDef* dyn_state_data_def         = necro_prim_def_data(prim_types, intern, &prim_types->dyn_state_type, necro_ast_create_data_declaration(&prim_types->arena, intern, dyn_state_s_type, dyn_state_constructor_list));
    UNUSED(dyn_state_data_def);

    // Eq
    NecroAst* eq_method_sig  = necro_create_class_comp_sig(&prim_types->arena, intern, "eq");
    NecroAst* neq_method_sig = necro_create_class_comp_sig(&prim_types->arena, intern, "neq");
    NecroAst* eq_method_list = necro_ast_create_decl(&prim_types->arena, eq_method_sig, necro_ast_create_decl(&prim_types->arena, neq_method_sig, NULL));
    NecroAst* eq_class_ast   = necro_ast_create_type_class(&prim_types->arena, intern, "Eq", "a", NULL, eq_method_list);
    NecroPrimDef* eq_class_def   = necro_prim_def_class(prim_types, intern, &prim_types->eq_type_class, eq_class_ast);
    UNUSED(eq_class_def);

    // Ord
    NecroAst* gt_method_sig   = necro_create_class_comp_sig(&prim_types->arena, intern, "gt");
    NecroAst* lt_method_sig   = necro_create_class_comp_sig(&prim_types->arena, intern, "lt");
    NecroAst* gte_method_sig  = necro_create_class_comp_sig(&prim_types->arena, intern, "gte");
    NecroAst* lte_method_sig  = necro_create_class_comp_sig(&prim_types->arena, intern, "lte");
    NecroAst* ord_method_list =
        necro_ast_create_decl(&prim_types->arena, gt_method_sig,
            necro_ast_create_decl(&prim_types->arena, lt_method_sig,
                necro_ast_create_decl(&prim_types->arena, gte_method_sig,
                    necro_ast_create_decl(&prim_types->arena, lte_method_sig, NULL))));
    NecroAst* ord_class_ast   = necro_ast_create_type_class(&prim_types->arena, intern, "Ord", "a", necro_ast_create_context(&prim_types->arena, intern, "Eq", "a", NULL), ord_method_list);
    NecroPrimDef* ord_class_def   = necro_prim_def_class(prim_types, intern, &prim_types->ord_type_class, ord_class_ast);
    UNUSED(ord_class_def);

    // Num
    NecroAst* add_method_sig    = necro_create_class_bin_op_sig(&prim_types->arena, intern, "add");
    NecroAst* sub_method_sig    = necro_create_class_bin_op_sig(&prim_types->arena, intern, "sub");
    NecroAst* mul_method_sig    = necro_create_class_bin_op_sig(&prim_types->arena, intern, "mul");
    NecroAst* abs_method_sig    = necro_create_class_unary_op_sig(&prim_types->arena, intern, "abs");
    NecroAst* signum_method_sig = necro_create_class_unary_op_sig(&prim_types->arena, intern, "signum");
    NecroAst* from_int_sig      =
        necro_ast_create_fn_type_sig(&prim_types->arena, intern, "fromInt", NULL, necro_ast_create_type_fn(&prim_types->arena,
            necro_ast_create_conid(&prim_types->arena, intern, "Int", NECRO_CON_TYPE_VAR),
            necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR)), NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
    NecroAst* num_method_list =
        necro_ast_create_decl(&prim_types->arena, add_method_sig,
            necro_ast_create_decl(&prim_types->arena, sub_method_sig,
                necro_ast_create_decl(&prim_types->arena, mul_method_sig,
                    necro_ast_create_decl(&prim_types->arena, abs_method_sig,
                        necro_ast_create_decl(&prim_types->arena, signum_method_sig,
                            necro_ast_create_decl(&prim_types->arena, from_int_sig, NULL))))));
    NecroAst* num_class_ast = necro_ast_create_type_class(&prim_types->arena, intern, "Num", "a", NULL, num_method_list);
    NecroPrimDef* num_class_def = necro_prim_def_class(prim_types, intern, &prim_types->num_type_class, num_class_ast);
    UNUSED(num_class_def);

    // Frac
    NecroAst* div_method_sig    = necro_create_class_bin_op_sig(&prim_types->arena, intern, "div");
    NecroAst* recip_method_sig  = necro_create_class_unary_op_sig(&prim_types->arena, intern, "recip");
    NecroAst* from_rational_sig =
        necro_ast_create_fn_type_sig(&prim_types->arena, intern, "fromRational", NULL, necro_ast_create_type_fn(&prim_types->arena,
            necro_ast_create_conid(&prim_types->arena, intern, "Rational", NECRO_CON_TYPE_VAR),
            necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR)), NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
    NecroAst* fractional_method_list =
        necro_ast_create_decl(&prim_types->arena, div_method_sig,
            necro_ast_create_decl(&prim_types->arena, recip_method_sig,
                necro_ast_create_decl(&prim_types->arena, from_rational_sig, NULL)));
    NecroAst* fractional_class_ast = necro_ast_create_type_class(&prim_types->arena, intern, "Fractional", "a", necro_ast_create_context(&prim_types->arena, intern, "Num", "a", NULL), fractional_method_list);
    NecroPrimDef* fractional_class_def = necro_prim_def_class(prim_types, intern, &prim_types->fractional_type_class, fractional_class_ast);
    UNUSED(fractional_class_def);

    // Prim Num Instances
    necro_create_prim_num_instances(prim_types, intern, "Int", false, false);
    necro_create_prim_num_instances(prim_types, intern, "Float", false, true);
    necro_create_prim_num_instances(prim_types, intern, "Audio", false, true);
    necro_create_prim_num_instances(prim_types, intern, "Rational", false, true);
    necro_create_prim_num_instances(prim_types, intern, "Pattern", true, true);
    necro_bool_instances(prim_types, intern);

    // (,)
    NecroAst* tuple_2_s_type      = necro_ast_create_simple_type(&prim_types->arena, intern, "(,)", necro_ast_create_var_list(&prim_types->arena, intern, 2, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroAst* tuple_2_constructor = necro_ast_create_data_con(&prim_types->arena, intern, "(,)", necro_ast_create_var_list(&prim_types->arena, intern, 2, NECRO_VAR_TYPE_FREE_VAR));
    NecroPrimDef* tuple_2_data_def    = necro_prim_def_data(prim_types, intern, &prim_types->tuple_types.two, necro_ast_create_data_declaration(&prim_types->arena, intern, tuple_2_s_type, necro_ast_create_list(&prim_types->arena, tuple_2_constructor, NULL)));
    UNUSED(tuple_2_data_def);

    // (,,)
    NecroAst* tuple_3_s_type      = necro_ast_create_simple_type(&prim_types->arena, intern, "(,,)", necro_ast_create_var_list(&prim_types->arena, intern, 3, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroAst* tuple_3_constructor = necro_ast_create_data_con(&prim_types->arena, intern, "(,,)", necro_ast_create_var_list(&prim_types->arena, intern, 3, NECRO_VAR_TYPE_FREE_VAR));
    NecroPrimDef* tuple_3_data_def    = necro_prim_def_data(prim_types, intern, &prim_types->tuple_types.three, necro_ast_create_data_declaration(&prim_types->arena, intern, tuple_3_s_type, necro_ast_create_list(&prim_types->arena, tuple_3_constructor, NULL)));
    UNUSED(tuple_3_data_def);

    // (,,,)
    NecroAst* tuple_4_s_type      = necro_ast_create_simple_type(&prim_types->arena, intern, "(,,,)", necro_ast_create_var_list(&prim_types->arena, intern, 4, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroAst* tuple_4_constructor = necro_ast_create_data_con(&prim_types->arena, intern, "(,,,)", necro_ast_create_var_list(&prim_types->arena, intern, 4, NECRO_VAR_TYPE_FREE_VAR));
    NecroPrimDef* tuple_4_data_def    = necro_prim_def_data(prim_types, intern, &prim_types->tuple_types.four, necro_ast_create_data_declaration(&prim_types->arena, intern, tuple_4_s_type, necro_ast_create_list(&prim_types->arena, tuple_4_constructor, NULL)));
    UNUSED(tuple_4_data_def);

    // (,,,,)
    NecroAst* tuple_5_s_type      = necro_ast_create_simple_type(&prim_types->arena, intern, "(,,,,)", necro_ast_create_var_list(&prim_types->arena, intern, 5, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroAst* tuple_5_constructor = necro_ast_create_data_con(&prim_types->arena, intern, "(,,,,)", necro_ast_create_var_list(&prim_types->arena, intern, 5, NECRO_VAR_TYPE_FREE_VAR));
    NecroPrimDef* tuple_5_data_def    = necro_prim_def_data(prim_types, intern, &prim_types->tuple_types.five, necro_ast_create_data_declaration(&prim_types->arena, intern, tuple_5_s_type, necro_ast_create_list(&prim_types->arena, tuple_5_constructor, NULL)));
    UNUSED(tuple_5_data_def);

    // (,,,,,)
    NecroAst* tuple_6_s_type      = necro_ast_create_simple_type(&prim_types->arena, intern, "(,,,,,)", necro_ast_create_var_list(&prim_types->arena, intern, 6, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroAst* tuple_6_constructor = necro_ast_create_data_con(&prim_types->arena, intern, "(,,,,,)", necro_ast_create_var_list(&prim_types->arena, intern, 6, NECRO_VAR_TYPE_FREE_VAR));
    NecroPrimDef* tuple_6_data_def    = necro_prim_def_data(prim_types, intern, &prim_types->tuple_types.six, necro_ast_create_data_declaration(&prim_types->arena, intern, tuple_6_s_type, necro_ast_create_list(&prim_types->arena, tuple_6_constructor, NULL)));
    UNUSED(tuple_6_data_def);

    // (,,,,,,)
    NecroAst* tuple_7_s_type      = necro_ast_create_simple_type(&prim_types->arena, intern, "(,,,,,,)", necro_ast_create_var_list(&prim_types->arena, intern, 7, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroAst* tuple_7_constructor = necro_ast_create_data_con(&prim_types->arena, intern, "(,,,,,,)", necro_ast_create_var_list(&prim_types->arena, intern, 7, NECRO_VAR_TYPE_FREE_VAR));
    NecroPrimDef* tuple_7_data_def    = necro_prim_def_data(prim_types, intern, &prim_types->tuple_types.seven, necro_ast_create_data_declaration(&prim_types->arena, intern, tuple_7_s_type, necro_ast_create_list(&prim_types->arena, tuple_7_constructor, NULL)));
    UNUSED(tuple_7_data_def);

    // (,,,,,,,)
    NecroAst* tuple_8_s_type      = necro_ast_create_simple_type(&prim_types->arena, intern, "(,,,,,,,)", necro_ast_create_var_list(&prim_types->arena, intern, 8, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroAst* tuple_8_constructor = necro_ast_create_data_con(&prim_types->arena, intern, "(,,,,,,,)", necro_ast_create_var_list(&prim_types->arena, intern, 8, NECRO_VAR_TYPE_FREE_VAR));
    NecroPrimDef* tuple_8_data_def    = necro_prim_def_data(prim_types, intern, &prim_types->tuple_types.eight, necro_ast_create_data_declaration(&prim_types->arena, intern, tuple_8_s_type, necro_ast_create_list(&prim_types->arena, tuple_8_constructor, NULL)));
    UNUSED(tuple_8_data_def);

    // (,,,,,,,,)
    NecroAst* tuple_9_s_type      = necro_ast_create_simple_type(&prim_types->arena, intern, "(,,,,,,,,)", necro_ast_create_var_list(&prim_types->arena, intern, 9, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroAst* tuple_9_constructor = necro_ast_create_data_con(&prim_types->arena, intern, "(,,,,,,,,)", necro_ast_create_var_list(&prim_types->arena, intern, 9, NECRO_VAR_TYPE_FREE_VAR));
    NecroPrimDef* tuple_9_data_def    = necro_prim_def_data(prim_types, intern, &prim_types->tuple_types.nine, necro_ast_create_data_declaration(&prim_types->arena, intern, tuple_9_s_type, necro_ast_create_list(&prim_types->arena, tuple_9_constructor, NULL)));
    UNUSED(tuple_9_data_def);

    // (,,,,,,,,,)
    NecroAst* tuple_10_s_type      = necro_ast_create_simple_type(&prim_types->arena, intern, "(,,,,,,,,,)", necro_ast_create_var_list(&prim_types->arena, intern, 10, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroAst* tuple_10_constructor = necro_ast_create_data_con(&prim_types->arena, intern, "(,,,,,,,,,)", necro_ast_create_var_list(&prim_types->arena, intern, 10, NECRO_VAR_TYPE_FREE_VAR));
    NecroPrimDef* tuple_10_data_def    = necro_prim_def_data(prim_types, intern, &prim_types->tuple_types.ten, necro_ast_create_data_declaration(&prim_types->arena, intern, tuple_10_s_type, necro_ast_create_list(&prim_types->arena, tuple_10_constructor, NULL)));
    UNUSED(tuple_10_data_def);

    // Functor
    {
        NecroAst* a_var          = necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* b_var          = necro_ast_create_var(&prim_types->arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* f_var          = necro_ast_create_var(&prim_types->arena, intern, "f", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* map_method_sig =
            necro_ast_create_fn_type_sig(&prim_types->arena, intern, "map", NULL,
                necro_ast_create_type_fn(&prim_types->arena,
                    necro_ast_create_type_fn(&prim_types->arena, a_var, b_var),
                    necro_ast_create_type_fn(&prim_types->arena,
                        necro_ast_create_type_app(&prim_types->arena, f_var, a_var),
                        necro_ast_create_type_app(&prim_types->arena, f_var, b_var))),
                    NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
        NecroAst* functor_method_list = necro_ast_create_decl(&prim_types->arena, map_method_sig, NULL);
        NecroAst* functor_class_ast   = necro_ast_create_type_class(&prim_types->arena, intern, "Functor", "f", NULL, functor_method_list);
        NecroPrimDef* functor_class_def   = necro_prim_def_class(prim_types, intern, &prim_types->functor_type_class, functor_class_ast);
        UNUSED(functor_class_def);
    }

    // Applicative
    {
        NecroAst* a_var           = necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* b_var           = necro_ast_create_var(&prim_types->arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* f_var           = necro_ast_create_var(&prim_types->arena, intern, "f", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* pure_method_sig =
            necro_ast_create_fn_type_sig(&prim_types->arena, intern, "pure", NULL, necro_ast_create_type_fn(&prim_types->arena, a_var, necro_ast_create_type_app(&prim_types->arena, f_var, a_var)), NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
        NecroAst* ap_method_sig  =
            necro_ast_create_fn_type_sig(&prim_types->arena, intern, "ap", NULL,
                necro_ast_create_type_fn(&prim_types->arena,
                    necro_ast_create_type_app(&prim_types->arena, f_var, necro_ast_create_type_fn(&prim_types->arena, a_var, b_var)),
                    necro_ast_create_type_fn(&prim_types->arena,
                        necro_ast_create_type_app(&prim_types->arena, f_var, a_var),
                        necro_ast_create_type_app(&prim_types->arena, f_var, b_var))),
                    NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
        NecroAst* applicative_method_list = necro_ast_create_decl(&prim_types->arena, pure_method_sig, necro_ast_create_decl(&prim_types->arena, ap_method_sig, NULL));
        NecroAst* applicative_class_ast   = necro_ast_create_type_class(&prim_types->arena, intern, "Applicative", "f", necro_ast_create_context(&prim_types->arena, intern, "Functor", "f", NULL), applicative_method_list);
        NecroPrimDef* applicative_class_def   = necro_prim_def_class(prim_types, intern, &prim_types->applicative_type_class, applicative_class_ast);
        UNUSED(applicative_class_def);
    }

    // Monad
    {
        NecroAst* a_var           = necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* b_var           = necro_ast_create_var(&prim_types->arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* m_var           = necro_ast_create_var(&prim_types->arena, intern, "m", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* bind_method_sig  =
            necro_ast_create_fn_type_sig(&prim_types->arena, intern, "bind", NULL,
                necro_ast_create_type_fn(&prim_types->arena,
                    necro_ast_create_type_app(&prim_types->arena, m_var, a_var),
                    necro_ast_create_type_fn(&prim_types->arena,
                        necro_ast_create_type_fn(&prim_types->arena, a_var, necro_ast_create_type_app(&prim_types->arena, m_var, b_var)),
                        necro_ast_create_type_app(&prim_types->arena, m_var, b_var))),
                    NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
        NecroAst* monad_method_list = necro_ast_create_decl(&prim_types->arena, bind_method_sig, NULL);
        NecroAst* monad_class_ast   = necro_ast_create_type_class(&prim_types->arena, intern, "Monad", "m", necro_ast_create_context(&prim_types->arena, intern, "Applicative", "m", NULL), monad_method_list);
        NecroPrimDef* monad_class_def   = necro_prim_def_class(prim_types, intern, &prim_types->monad_type_class, monad_class_ast);
        UNUSED(monad_class_def);
    }

    // Semigroup
    {
        NecroAst* a_var             = necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* append_method_sig =
            necro_ast_create_fn_type_sig(&prim_types->arena, intern, "append", NULL,
                necro_ast_create_type_fn(&prim_types->arena, a_var, necro_ast_create_type_fn(&prim_types->arena, a_var, a_var)),
                    NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
        NecroAst* semigroup_method_list = necro_ast_create_decl(&prim_types->arena, append_method_sig, NULL);
        NecroAst* semigroup_class_ast   = necro_ast_create_type_class(&prim_types->arena, intern, "Semigroup", "a", NULL, semigroup_method_list);
        NecroPrimDef* semigroup_class_def   = necro_prim_def_class(prim_types, intern, NULL, semigroup_class_ast);
        UNUSED(semigroup_class_def);
    }

    // Monoid
    {
        NecroAst* a_var             = necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* mempty_method_sig =
            necro_ast_create_fn_type_sig(&prim_types->arena, intern, "mempty", NULL,
                a_var,
                    NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
        NecroAst* monoid_method_list = necro_ast_create_decl(&prim_types->arena, mempty_method_sig, NULL);
        NecroAst* monoid_class_ast   = necro_ast_create_type_class(&prim_types->arena, intern, "Monoid", "a", necro_ast_create_context(&prim_types->arena, intern, "Semigroup", "a", NULL), monoid_method_list);
        NecroPrimDef* monoid_class_def   = necro_prim_def_class(prim_types, intern, NULL, monoid_class_ast);
        UNUSED(monoid_class_def);
    }

    //=====================================================
    // BinOps
    //=====================================================

    // +
    NecroAst* add_sig_ast  = necro_create_num_bin_op_sig(&prim_types->arena, intern, "+");
    NecroAst* add_def_ast  = necro_ast_create_simple_assignment(&prim_types->arena, intern, "+", necro_ast_create_var(&prim_types->arena, intern, "add", NECRO_VAR_VAR));
    NecroPrimDef* add_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, add_sig_ast, add_def_ast);
    UNUSED(add_prim_def);

    // -
    NecroAst* sub_sig_ast  = necro_create_num_bin_op_sig(&prim_types->arena, intern, "-");
    NecroAst* sub_def_ast  = necro_ast_create_simple_assignment(&prim_types->arena, intern, "-", necro_ast_create_var(&prim_types->arena, intern, "sub", NECRO_VAR_VAR));
    NecroPrimDef* sub_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, sub_sig_ast, sub_def_ast);
    UNUSED(sub_prim_def);

    // *
    NecroAst* mul_sig_ast  = necro_create_num_bin_op_sig(&prim_types->arena, intern, "*");
    NecroAst* mul_def_ast  = necro_ast_create_simple_assignment(&prim_types->arena, intern, "*", necro_ast_create_var(&prim_types->arena, intern, "mul", NECRO_VAR_VAR));
    NecroPrimDef* mul_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, mul_sig_ast, mul_def_ast);
    UNUSED(mul_prim_def);

    // /
    NecroAst* div_sig_ast  = necro_create_fractional_bin_op_sig(&prim_types->arena, intern, "/");
    NecroAst* div_def_ast  = necro_ast_create_simple_assignment(&prim_types->arena, intern, "/", necro_ast_create_var(&prim_types->arena, intern, "div", NECRO_VAR_VAR));
    NecroPrimDef* div_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, div_sig_ast, div_def_ast);
    UNUSED(div_prim_def);

    // ==
    NecroAst* eq_sig_ast  = necro_create_eq_comp_sig(&prim_types->arena, intern, "==");
    NecroAst* eq_def_ast  = necro_ast_create_simple_assignment(&prim_types->arena, intern, "==", necro_ast_create_var(&prim_types->arena, intern, "eq", NECRO_VAR_VAR));
    NecroPrimDef* eq_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, eq_sig_ast, eq_def_ast);
    UNUSED(eq_prim_def);

    // /=
    NecroAst* neq_sig_ast  = necro_create_eq_comp_sig(&prim_types->arena, intern, "/=");
    NecroAst* neq_def_ast  = necro_ast_create_simple_assignment(&prim_types->arena, intern, "/=", necro_ast_create_var(&prim_types->arena, intern, "neq", NECRO_VAR_VAR));
    NecroPrimDef* neq_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, neq_sig_ast, neq_def_ast);
    UNUSED(neq_prim_def);

    // <
    NecroAst* lt_sig_ast  = necro_create_ord_comp_sig(&prim_types->arena, intern, "<");
    NecroAst* lt_def_ast  = necro_ast_create_simple_assignment(&prim_types->arena, intern, "<", necro_ast_create_var(&prim_types->arena, intern, "lt", NECRO_VAR_VAR));
    NecroPrimDef* lt_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, lt_sig_ast, lt_def_ast);
    UNUSED(lt_prim_def);

    // >
    NecroAst* gt_sig_ast  = necro_create_ord_comp_sig(&prim_types->arena, intern, ">");
    NecroAst* gt_def_ast  = necro_ast_create_simple_assignment(&prim_types->arena, intern, ">", necro_ast_create_var(&prim_types->arena, intern, "gt", NECRO_VAR_VAR));
    NecroPrimDef* gt_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, gt_sig_ast, gt_def_ast);
    UNUSED(gt_prim_def);

    // <=
    NecroAst* lte_sig_ast  = necro_create_ord_comp_sig(&prim_types->arena, intern, "<=");
    NecroAst* lte_def_ast  = necro_ast_create_simple_assignment(&prim_types->arena, intern, "<=", necro_ast_create_var(&prim_types->arena, intern, "lte", NECRO_VAR_VAR));
    NecroPrimDef* lte_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, lte_sig_ast, lte_def_ast);
    UNUSED(lte_prim_def);

    // >=
    NecroAst* gte_sig_ast  = necro_create_ord_comp_sig(&prim_types->arena, intern, ">=");
    NecroAst* gte_def_ast  = necro_ast_create_simple_assignment(&prim_types->arena, intern, ">=", necro_ast_create_var(&prim_types->arena, intern, "gte", NECRO_VAR_VAR));
    NecroPrimDef* gte_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, gte_sig_ast, gte_def_ast);
    UNUSED(gte_prim_def);

    // >>=
    {
        NecroAst* a_var       = necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* b_var       = necro_ast_create_var(&prim_types->arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* m_var       = necro_ast_create_var(&prim_types->arena, intern, "m", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* bind_op_sig =
            necro_ast_create_fn_type_sig(&prim_types->arena, intern, ">>=", necro_ast_create_context(&prim_types->arena, intern, "Monad", "m", NULL),
                necro_ast_create_type_fn(&prim_types->arena,
                    necro_ast_create_type_app(&prim_types->arena, m_var, a_var),
                    necro_ast_create_type_fn(&prim_types->arena,
                        necro_ast_create_type_fn(&prim_types->arena, a_var, necro_ast_create_type_app(&prim_types->arena, m_var, b_var)),
                        necro_ast_create_type_app(&prim_types->arena, m_var, b_var))),
                NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        NecroAst* bind_def_ast = necro_ast_create_simple_assignment(&prim_types->arena, intern, ">>=", necro_ast_create_var(&prim_types->arena, intern, "bind", NECRO_VAR_VAR));
        NecroPrimDef* bind_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, bind_op_sig, bind_def_ast);
        UNUSED(bind_prim_def);
    }

    // >>
    {
        NecroAst* a_var       = necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* b_var       = necro_ast_create_var(&prim_types->arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* m_var       = necro_ast_create_var(&prim_types->arena, intern, "m", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* op_sig =
            necro_ast_create_fn_type_sig(&prim_types->arena, intern, ">>", necro_ast_create_context(&prim_types->arena, intern, "Monad", "m", NULL),
                necro_ast_create_type_fn(&prim_types->arena,
                    necro_ast_create_type_app(&prim_types->arena, m_var, a_var),
                    necro_ast_create_type_fn(&prim_types->arena,
                        necro_ast_create_type_app(&prim_types->arena, m_var, b_var),
                        necro_ast_create_type_app(&prim_types->arena, m_var, b_var))),
                NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        NecroAst* mm_var      = necro_ast_create_var(&prim_types->arena, intern, "m", NECRO_VAR_DECLARATION);
        NecroAst* mk_var      = necro_ast_create_var(&prim_types->arena, intern, "k", NECRO_VAR_DECLARATION);
        NecroAst* op_args     = necro_ast_create_apats(&prim_types->arena, mm_var, necro_ast_create_apats(&prim_types->arena, mk_var, NULL));
        NecroAst* op_lambda   = necro_ast_create_bin_op(&prim_types->arena, intern, ">>=",
            necro_ast_create_var(&prim_types->arena, intern, "m", NECRO_VAR_VAR),
            necro_ast_create_lambda(&prim_types->arena, necro_ast_create_apats(&prim_types->arena, necro_ast_create_wildcard(&prim_types->arena), NULL), necro_ast_create_var(&prim_types->arena, intern, "k", NECRO_VAR_VAR)));
        NecroAst* op_rhs      = necro_ast_create_rhs(&prim_types->arena, op_lambda, NULL);
        NecroAst* op_def_ast  = necro_ast_create_apats_assignment(&prim_types->arena, intern, ">>", op_args, op_rhs);
        NecroPrimDef* op_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, op_sig, op_def_ast);
        UNUSED(op_prim_def);
    }

    // <>
    {
        NecroAst* a_var         = necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* append_op_sig =
            necro_ast_create_fn_type_sig(&prim_types->arena, intern, "<>", necro_ast_create_context(&prim_types->arena, intern, "Semigroup", "a", NULL),
                necro_ast_create_type_fn(&prim_types->arena, a_var, necro_ast_create_type_fn(&prim_types->arena, a_var, a_var)),
                NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        NecroAst* append_def_ast  = necro_ast_create_simple_assignment(&prim_types->arena, intern, "<>", necro_ast_create_var(&prim_types->arena, intern, "append", NECRO_VAR_VAR));
        NecroPrimDef* append_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, append_op_sig, append_def_ast);
        UNUSED(append_prim_def);
    }

    // <|
    {
        NecroAst* a_var       = necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* b_var       = necro_ast_create_var(&prim_types->arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* op_sig      =
            necro_ast_create_fn_type_sig(&prim_types->arena, intern, "<|", NULL,
                necro_ast_create_type_fn(&prim_types->arena, necro_ast_create_type_fn(&prim_types->arena, a_var, b_var),
                    necro_ast_create_type_fn(&prim_types->arena, a_var, b_var)),
                NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        NecroAst* f_var       = necro_ast_create_var(&prim_types->arena, intern, "f", NECRO_VAR_DECLARATION);
        NecroAst* x_var       = necro_ast_create_var(&prim_types->arena, intern, "x", NECRO_VAR_DECLARATION);
        NecroAst* op_args     = necro_ast_create_apats(&prim_types->arena, f_var, necro_ast_create_apats(&prim_types->arena, x_var, NULL));
        NecroAst* op_rhs_ast  = necro_ast_create_rhs(&prim_types->arena, necro_ast_create_fexpr(&prim_types->arena, necro_ast_create_var(&prim_types->arena, intern, "f", NECRO_VAR_VAR), necro_ast_create_var(&prim_types->arena, intern, "x", NECRO_VAR_VAR)), NULL);
        NecroAst* op_def_ast  = necro_ast_create_apats_assignment(&prim_types->arena, intern, "<|", op_args, op_rhs_ast);
        NecroPrimDef* op_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, op_sig, op_def_ast);
        UNUSED(op_prim_def);
    }

    // |>
    {
        NecroAst* a_var       = necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* b_var       = necro_ast_create_var(&prim_types->arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* op_sig      =
            necro_ast_create_fn_type_sig(&prim_types->arena, intern, "|>", NULL,
                necro_ast_create_type_fn(&prim_types->arena, a_var,
                    necro_ast_create_type_fn(&prim_types->arena, necro_ast_create_type_fn(&prim_types->arena, a_var, b_var), b_var)),
                NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        NecroAst* x_var       = necro_ast_create_var(&prim_types->arena, intern, "x", NECRO_VAR_DECLARATION);
        NecroAst* f_var       = necro_ast_create_var(&prim_types->arena, intern, "f", NECRO_VAR_DECLARATION);
        NecroAst* op_args     = necro_ast_create_apats(&prim_types->arena, x_var, necro_ast_create_apats(&prim_types->arena, f_var, NULL));
        NecroAst* op_rhs_ast  = necro_ast_create_rhs(&prim_types->arena, necro_ast_create_fexpr(&prim_types->arena, necro_ast_create_var(&prim_types->arena, intern, "f", NECRO_VAR_VAR), necro_ast_create_var(&prim_types->arena, intern, "x", NECRO_VAR_VAR)), NULL);
        NecroAst* op_def_ast  = necro_ast_create_apats_assignment(&prim_types->arena, intern, "|>", op_args, op_rhs_ast);
        NecroPrimDef* op_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, op_sig, op_def_ast);
        UNUSED(op_prim_def);
    }

    // .
    {
        NecroAst* a_var       = necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* b_var       = necro_ast_create_var(&prim_types->arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* c_var       = necro_ast_create_var(&prim_types->arena, intern, "c", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* op_sig      =
            necro_ast_create_fn_type_sig(&prim_types->arena, intern, ".", NULL,
                necro_ast_create_type_fn(&prim_types->arena, necro_ast_create_type_fn(&prim_types->arena, b_var, c_var),
                    necro_ast_create_type_fn(&prim_types->arena, necro_ast_create_type_fn(&prim_types->arena, a_var, b_var),
                        necro_ast_create_type_fn(&prim_types->arena, a_var, c_var))),
                NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        NecroAst* f_var       = necro_ast_create_var(&prim_types->arena, intern, "f", NECRO_VAR_DECLARATION);
        NecroAst* g_var       = necro_ast_create_var(&prim_types->arena, intern, "g", NECRO_VAR_DECLARATION);
        NecroAst* x_var       = necro_ast_create_var(&prim_types->arena, intern, "x", NECRO_VAR_DECLARATION);
        NecroAst* op_args     = necro_ast_create_apats(&prim_types->arena, f_var, necro_ast_create_apats(&prim_types->arena, g_var, NULL));
        NecroAst* op_lambda   = necro_ast_create_lambda(&prim_types->arena, necro_ast_create_apats(&prim_types->arena, x_var, NULL),
            necro_ast_create_fexpr(&prim_types->arena, necro_ast_create_var(&prim_types->arena, intern, "f", NECRO_VAR_VAR),
                necro_ast_create_fexpr(&prim_types->arena, necro_ast_create_var(&prim_types->arena, intern, "g", NECRO_VAR_VAR),
                    necro_ast_create_var(&prim_types->arena, intern, "x", NECRO_VAR_VAR))));
        NecroAst* op_rhs_ast  = necro_ast_create_rhs(&prim_types->arena, op_lambda, NULL);
        NecroAst* op_def_ast  = necro_ast_create_apats_assignment(&prim_types->arena, intern, ".", op_args, op_rhs_ast);
        NecroPrimDef* op_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, op_sig, op_def_ast);
        UNUSED(op_prim_def);
    }


    {
        // delay
        NecroAst* a_var         = necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* delay_sig_ast = necro_ast_create_fn_type_sig(&prim_types->arena, intern, "delay", NULL,
            necro_ast_create_type_fn(&prim_types->arena, a_var,
                necro_ast_create_type_fn(&prim_types->arena, a_var, a_var)), NECRO_VAR_DECLARATION, NECRO_SIG_DECLARATION);
        necro_prim_def_fun(prim_types, intern, &prim_types->delay_fn, delay_sig_ast, 2);
    }

    // world value
    {
        NecroAst* world_value_sig_ast = necro_ast_create_fn_type_sig(&prim_types->arena, intern, "world", NULL,
            necro_ast_create_conid(&prim_types->arena, intern, "World", NECRO_CON_TYPE_VAR), NECRO_VAR_DECLARATION, NECRO_SIG_DECLARATION);
        necro_prim_def_fun(prim_types, intern, &prim_types->world_value, world_value_sig_ast, 0);
    }

    {
        // mouseX
        NecroAst* mouse_x_sig_ast = necro_ast_create_fn_type_sig(&prim_types->arena, intern, "getMouseX", NULL,
            necro_ast_create_type_fn(&prim_types->arena, necro_ast_create_conid(&prim_types->arena, intern, "World", NECRO_CON_TYPE_VAR), necro_ast_create_conid(&prim_types->arena, intern, "Int", NECRO_CON_TYPE_VAR)),
            NECRO_VAR_DECLARATION, NECRO_SIG_DECLARATION);
        necro_prim_def_fun(prim_types, intern, &prim_types->mouse_x_fn, mouse_x_sig_ast, 1);
    }

    {
        // mouseY
        NecroAst* mouse_y_sig_ast = necro_ast_create_fn_type_sig(&prim_types->arena, intern, "getMouseY", NULL,
            necro_ast_create_type_fn(&prim_types->arena, necro_ast_create_conid(&prim_types->arena, intern, "World", NECRO_CON_TYPE_VAR), necro_ast_create_conid(&prim_types->arena, intern, "Int", NECRO_CON_TYPE_VAR)),
            NECRO_VAR_DECLARATION, NECRO_SIG_DECLARATION);
        necro_prim_def_fun(prim_types, intern, &prim_types->mouse_y_fn, mouse_y_sig_ast, 1);
    }

    // unsafeMalloc
    {
        NecroAst* unsafe_malloc_sig_ast = necro_ast_create_fn_type_sig(&prim_types->arena, intern, "unsafeMalloc", NULL,
            necro_ast_create_type_fn(&prim_types->arena, necro_ast_create_conid(&prim_types->arena, intern, "Int", NECRO_CON_TYPE_VAR),
                necro_ast_create_type_app(&prim_types->arena,
                    necro_ast_create_conid(&prim_types->arena, intern, "Ptr", NECRO_CON_TYPE_VAR),
                    necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR))
                ),
            NECRO_VAR_DECLARATION, NECRO_SIG_DECLARATION);
        necro_prim_def_fun(prim_types, intern, &prim_types->unsafe_malloc, unsafe_malloc_sig_ast, 1);
    }

    // unsafePeek
    {
        NecroAst* unsafe_peek_sig_ast = necro_ast_create_fn_type_sig(&prim_types->arena, intern, "unsafePeek", NULL,
            necro_ast_create_type_fn(&prim_types->arena,
                necro_ast_create_conid(&prim_types->arena, intern, "Int", NECRO_CON_TYPE_VAR),
                necro_ast_create_type_fn(&prim_types->arena,
                    necro_ast_create_type_app(&prim_types->arena,
                        necro_ast_create_conid(&prim_types->arena, intern, "Ptr", NECRO_CON_TYPE_VAR),
                        necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR)),
                    necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR))
                ),
            NECRO_VAR_DECLARATION, NECRO_SIG_DECLARATION);
        necro_prim_def_fun(prim_types, intern, &prim_types->unsafe_peek, unsafe_peek_sig_ast, 2);
    }

    // unsafePoke
    {
        NecroAst* unsafe_poke_sig_ast = necro_ast_create_fn_type_sig(&prim_types->arena, intern, "unsafePoke", NULL,
            necro_ast_create_type_fn(&prim_types->arena,
                necro_ast_create_conid(&prim_types->arena, intern, "Int", NECRO_CON_TYPE_VAR),
                necro_ast_create_type_fn(&prim_types->arena,
                    necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                    necro_ast_create_type_fn(&prim_types->arena,
                        necro_ast_create_type_app(&prim_types->arena,
                            necro_ast_create_conid(&prim_types->arena, intern, "Ptr", NECRO_CON_TYPE_VAR),
                            necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR)),
                        necro_ast_create_type_app(&prim_types->arena,
                            necro_ast_create_conid(&prim_types->arena, intern, "Ptr", NECRO_CON_TYPE_VAR),
                            necro_ast_create_var(&prim_types->arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR))))),
            NECRO_VAR_DECLARATION, NECRO_SIG_DECLARATION);
        necro_prim_def_fun(prim_types, intern, &prim_types->unsafe_poke, unsafe_poke_sig_ast, 3);
    }

    // plusPtr

    // && / ||
    {
        NecroAst* bool_conid   = necro_ast_create_conid(&prim_types->arena, intern, "Bool", NECRO_CON_TYPE_VAR);
        NecroAst* and_sig_ast  =
            necro_ast_create_fn_type_sig(&prim_types->arena, intern, "&&", NULL,
                necro_ast_create_type_fn(&prim_types->arena,
                    bool_conid,
                    necro_ast_create_type_fn(&prim_types->arena,
                        bool_conid,
                        bool_conid)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        NecroAst* and_def_ast  = necro_ast_create_simple_assignment(&prim_types->arena, intern, "&&", necro_ast_create_var(&prim_types->arena, intern, "eq@Bool", NECRO_VAR_VAR));
        NecroPrimDef* and_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, and_sig_ast, and_def_ast);
        UNUSED(and_prim_def);
        // NecroPrimDef* and_fun_def  = necro_prim_def_fun(prim_types, intern, NULL, and_sig_ast, 2);
        NecroAst* or_sig_ast  =
            necro_ast_create_fn_type_sig(&prim_types->arena, intern, "||", NULL,
                necro_ast_create_type_fn(&prim_types->arena,
                    bool_conid,
                    necro_ast_create_type_fn(&prim_types->arena,
                        bool_conid,
                        bool_conid)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        NecroAst* or_def_ast  = necro_ast_create_simple_assignment(&prim_types->arena, intern, "||", necro_ast_create_var(&prim_types->arena, intern, "neq@Bool", NECRO_VAR_VAR));
        NecroPrimDef* or_prim_def = necro_prim_def_bin_op(prim_types, intern, NULL, or_sig_ast, or_def_ast);
        UNUSED(or_prim_def);
        // NecroPrimDef* or_fun_def  = necro_prim_def_fun(prim_types, intern, NULL, or_sig_ast, 2);
    }

}

NecroCon necro_prim_types_get_data_con_from_symbol(NecroPrimTypes* prim_types, NecroSymbol symbol)
{
    return *necro_con_table_get(&prim_types->con_table, symbol->symbol_num);
}

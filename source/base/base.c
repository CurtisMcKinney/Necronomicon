/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include "base.h"
#include "renamer.h"
#include "d_analyzer.h"
#include "kind.h"
#include "infer.h"
#include "monomorphize.h"
#include "alias_analysis.h"

///////////////////////////////////////////////////////
// Create / Destroy
///////////////////////////////////////////////////////
NecroBase necro_base_create()
{
    NecroBase base;
    memset(&base, 0, sizeof(NecroBase));
    base.ast = necro_ast_arena_empty();
    return base;
}

void necro_base_destroy(NecroBase* base)
{
    necro_ast_arena_destroy(&base->ast);
}

void necro_append_top(NecroPagedArena* arena, NecroAst* top, NecroAst* item)
{
    assert(top != NULL);
    assert(top->type == NECRO_AST_TOP_DECL);
    while (top->top_declaration.next_top_decl != NULL)
    {
        assert(top->type == NECRO_AST_TOP_DECL);
        top = top->top_declaration.next_top_decl;
    }
    top->top_declaration.next_top_decl = necro_ast_create_top_decl(arena, item, NULL);
}

void necro_append_top_decl(NecroAst* top, NecroAst* next_top)
{
    assert(top != NULL);
    assert(top->type == NECRO_AST_TOP_DECL);
    while (top->top_declaration.next_top_decl != NULL)
    {
        assert(top->type == NECRO_AST_TOP_DECL);
        top = top->top_declaration.next_top_decl;
    }
    top->top_declaration.next_top_decl = next_top;
}

void necro_base_create_simple_data_decl(NecroPagedArena* arena, NecroAst* top, NecroIntern* intern, const char* data_type_name, const char* data_type_prim_con_name)
{
    NecroAst* s_type   = necro_ast_create_simple_type(arena, intern, data_type_name, NULL);
    NecroAst* n_con    = necro_ast_create_data_con(arena, intern, data_type_prim_con_name, NULL);
    NecroAst* con_list = necro_ast_create_list(arena, n_con, NULL);
    necro_append_top(arena, top, necro_ast_create_data_declaration(arena, intern, s_type, con_list));
}

void necro_base_create_simple_poly_data_decl(NecroPagedArena* arena, NecroAst* top, NecroIntern* intern, const char* data_type_name)
{
    NecroAst* s_type   = necro_ast_create_simple_type(arena, intern, data_type_name, necro_ast_create_var_list(arena, intern, 1, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroAst* n_con    = necro_ast_create_data_con(arena, intern, data_type_name, necro_ast_create_var_list(arena, intern, 1, NECRO_VAR_TYPE_FREE_VAR));
    NecroAst* con_list = necro_ast_create_list(arena, n_con, NULL);
    necro_append_top(arena, top, necro_ast_create_data_declaration(arena, intern, s_type, con_list));
}

NecroAst* necro_base_create_class_comp_sig(NecroPagedArena* arena, NecroIntern* intern, const char* sig_name)
{
    return
        necro_ast_create_fn_type_sig(arena, intern, sig_name, NULL,
            necro_ast_create_type_fn(arena,
                necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                necro_ast_create_type_fn(arena,
                    necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                    necro_ast_create_conid(arena, intern, "Bool", NECRO_CON_TYPE_VAR))), NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
}

NecroAst* necro_base_create_class_bin_op_sig(NecroPagedArena* arena, NecroIntern* intern, const char* sig_name)
{
    return
        necro_ast_create_fn_type_sig(arena, intern, sig_name, NULL,
            necro_ast_create_type_fn(arena,
                necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                necro_ast_create_type_fn(arena,
                    necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                    necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR))), NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
}


NecroAst* necro_base_create_class_unary_op_sig(NecroPagedArena* arena, NecroIntern* intern, const char* sig_name)
{
    return
        necro_ast_create_fn_type_sig(arena, intern, sig_name, NULL,
            necro_ast_create_type_fn(arena,
                necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR)), NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
}

NecroAst* necro_base_make_poly_con1(NecroPagedArena* arena, NecroIntern* intern, const char* data_type_name)
{
    NecroAst* type_var_ast = necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
    NecroAst* type_constr  = necro_ast_create_data_con(arena, intern, data_type_name, necro_ast_create_list(arena, type_var_ast, NULL));
    type_constr->constructor.conid->conid.con_type = NECRO_CON_TYPE_VAR;
    return type_constr;
}

NecroAst* necro_base_make_num_con(NecroPagedArena* arena, NecroIntern* intern, const char* data_type_name, bool is_polymorphic)
{
    if (is_polymorphic)
        return necro_base_make_poly_con1(arena, intern, data_type_name);
    else
        return necro_ast_create_conid(arena, intern, data_type_name, NECRO_CON_TYPE_VAR);
}

NecroAst* necro_base_make_bin_math_ast(NecroPagedArena* arena, NecroIntern* intern, const char* data_type_name, bool is_poly)
{
    return necro_ast_create_type_fn(arena,
        necro_base_make_num_con(arena, intern, data_type_name, is_poly),
        necro_ast_create_type_fn(arena,
            necro_base_make_num_con(arena, intern, data_type_name, is_poly),
            necro_base_make_num_con(arena, intern, data_type_name, is_poly)));
}

NecroAst* necro_base_make_unary_math_ast(NecroPagedArena* arena, NecroIntern* intern, const char* data_type_name, bool is_poly)
{
    return necro_ast_create_type_fn(arena,
        necro_base_make_num_con(arena, intern, data_type_name, is_poly),
        necro_base_make_num_con(arena, intern, data_type_name, is_poly));
}

NecroAst* necro_base_make_num_context(NecroPagedArena* arena, NecroIntern* intern, const char* class_name, bool is_polymorphic)
{
    if (is_polymorphic)
        return necro_ast_create_context(arena, intern, class_name, "a", NULL);
    else
        return NULL;
}

// NecroAst* necro_base_create_num_bin_op_sig(NecroPagedArena* arena, NecroIntern* intern, const char* sig_name)
// {
//     return
//         necro_ast_create_fn_type_sig(arena, intern, sig_name, necro_ast_create_context(arena, intern, "Num", "a", NULL),
//             necro_ast_create_type_fn(arena,
//                 necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
//                 necro_ast_create_type_fn(arena,
//                     necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
//                     necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR))), NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
// }
//
// NecroAst* necro_base_create_fractional_bin_op_sig(NecroPagedArena* arena, NecroIntern* intern, const char* sig_name)
// {
//     return
//         necro_ast_create_fn_type_sig(arena, intern, sig_name, necro_ast_create_context(arena, intern, "Fractional", "a", NULL),
//             necro_ast_create_type_fn(arena,
//                 necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
//                 necro_ast_create_type_fn(arena,
//                     necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
//                     necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR))), NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
// }

NecroAst* necro_base_create_eq_comp_sig(NecroPagedArena* arena, NecroIntern* intern, const char* sig_name)
{
    return
        necro_ast_create_fn_type_sig(arena, intern, sig_name, necro_ast_create_context(arena, intern, "Eq", "a", NULL),
            necro_ast_create_type_fn(arena,
                necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                necro_ast_create_type_fn(arena,
                    necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                    necro_ast_create_conid(arena, intern, "Bool", NECRO_CON_TYPE_VAR))), NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
}

NecroAst* necro_base_create_ord_comp_sig(NecroPagedArena* arena, NecroIntern* intern, const char* sig_name)
{
    return
        necro_ast_create_fn_type_sig(arena, intern, sig_name, necro_ast_create_context(arena, intern, "Ord", "a", NULL),
            necro_ast_create_type_fn(arena,
                necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                necro_ast_create_type_fn(arena,
                    necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                    necro_ast_create_conid(arena, intern, "Bool", NECRO_CON_TYPE_VAR))), NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
}

NecroAst* necro_ast_create_prim_bin_op_method_ast(NecroPagedArena* arena, NecroIntern* intern, const char* name)
{
    return necro_ast_create_apats_assignment(arena, intern, name,
        necro_ast_create_apats(arena,
            necro_ast_create_var(arena, intern, "x", NECRO_VAR_DECLARATION),
            necro_ast_create_apats(arena,
                necro_ast_create_var(arena, intern, "y", NECRO_VAR_DECLARATION),
                NULL)),
        necro_ast_create_rhs(arena,
            necro_ast_create_var(arena, intern, "primUndefined", NECRO_VAR_VAR),
            NULL));
}

NecroAst* necro_ast_create_prim_unary_op_method_ast(NecroPagedArena* arena, NecroIntern* intern, const char* name)
{
    return necro_ast_create_apats_assignment(arena, intern, name,
        necro_ast_create_apats(arena,
            necro_ast_create_var(arena, intern, "x", NECRO_VAR_DECLARATION),
            NULL),
        necro_ast_create_rhs(arena,
            necro_ast_create_var(arena, intern, "primUndefined", NECRO_VAR_VAR),
            NULL));
}

void necro_base_setup_primitive(NecroScopedSymTable* scoped_symtable, NecroIntern* intern, const char* prim_name, NecroAstSymbol** symbol_to_cache, NECRO_PRIMOP_TYPE primop_type)
{
    NecroAstSymbol* symbol = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, prim_name));
    assert(symbol != NULL);
    symbol->is_primitive = true;
    symbol->primop_type  = primop_type;
    if (symbol_to_cache != NULL)
    {
        *symbol_to_cache = symbol;
    }
}

char* necro_base_open_lib_file(const char* file_name, size_t* file_length)
{
#ifdef WIN32
        FILE* file;
        fopen_s(&file, file_name, "r");
#else
        FILE* file = fopen(file_name, "r");
#endif
        if (!file)
        {
            return NULL;
        }

        char*  str    = NULL;
        size_t length = 0;

        // Find length of file
        fseek(file, 0, SEEK_END);
        length = ftell(file);
        fseek(file, 0, SEEK_SET);

        // Allocate buffer
        str = emalloc(length + 2);

        // read contents of buffer
        length = fread(str, 1, length, file);
        str[length]     = '\n';
        str[length + 1] = '\0';
        fclose(file);
        *file_length    = length;
        return str;
}

// NOTE: We load the contents of base lib file at start up so that we don't need to reload it every time we run a test, which slows things down a lot.
size_t necro_base_lib_string_length = 0;
char*  necro_base_lib_string        = NULL;
void necro_base_global_init()
{
    necro_base_lib_string                                    = necro_base_open_lib_file("./lib/base.necro", &necro_base_lib_string_length);
    if (necro_base_lib_string == NULL) necro_base_lib_string = necro_base_open_lib_file("../lib/base.necro", &necro_base_lib_string_length);
    if (necro_base_lib_string == NULL) necro_base_lib_string = necro_base_open_lib_file("../../lib/base.necro", &necro_base_lib_string_length);
    if (necro_base_lib_string == NULL) necro_base_lib_string = necro_base_open_lib_file("../../../lib/base.necro", &necro_base_lib_string_length);
    if (necro_base_lib_string == NULL) necro_base_lib_string = necro_base_open_lib_file("./base.necro", &necro_base_lib_string_length);
}

void necro_base_global_cleanup()
{
    free(necro_base_lib_string);
}

NecroBase necro_base_compile(NecroIntern* intern, NecroScopedSymTable* scoped_symtable)
{
    NecroCompileInfo info             = (NecroCompileInfo) { .verbosity = 0, .timer = NULL, .opt_level = NECRO_OPT_OFF, .compilation_phase = NECRO_PHASE_JIT };
    NecroBase        base             = necro_base_create();
    const char*      base_source_name = "base.necro";

    //--------------------
    // base.necro
    NecroLexTokenVector lex_tokens = necro_empty_lex_token_vector();
    NecroParseAstArena  parse_ast  = necro_parse_ast_arena_empty();
    unwrap_result_or_print_error(void, necro_lex(info, intern, necro_base_lib_string, necro_base_lib_string_length, &lex_tokens), necro_base_lib_string, base_source_name);
    unwrap_result_or_print_error(void, necro_parse(info, intern, &lex_tokens, necro_intern_string(intern, "Necro.Base"), &parse_ast), necro_base_lib_string, base_source_name);
    base.ast                       = necro_reify(info, intern, &parse_ast);
    NecroAst*           file_top   = base.ast.root;
    NecroPagedArena*    arena      = &base.ast.arena;
    NecroAst*           top        = NULL;
    necro_parse_ast_arena_destroy(&parse_ast);
    necro_destroy_lex_token_vector(&lex_tokens);

    necro_kind_init_kinds(&base, scoped_symtable, intern);

    /*
        HACK: Magical BlockSize :: Nat data type
        Magically appears as the value of the current block size, fixed at compile time.
        We need to actually funnel the real block size into this...
        Probably should create a better way of handling this...
    */
    NecroAstSymbol* block_size_symbol     = necro_ast_symbol_create(arena, necro_intern_string(intern, "Necro.Base.BlockSize"), necro_intern_string(intern, "BlockSize"), necro_intern_string(intern, "Necro.Base"), NULL);
    block_size_symbol->type               = necro_type_con_create(arena, block_size_symbol, NULL);
    block_size_symbol->type->kind         = base.nat_kind->type;
    block_size_symbol->type->pre_supplied = true;
    necro_scope_insert_ast_symbol(arena, scoped_symtable->top_type_scope, block_size_symbol);

    /*
        HACK: Magical SampleRate :: Nat data type
        Magically appears as the value of the current sample rate, fixed at compile time.
        We need to actually funnel the real sample rate into this...
        Probably should create a better way of handling this...
    */
    NecroAstSymbol* sample_rate_symbol     = necro_ast_symbol_create(arena, necro_intern_string(intern, "Necro.Base.SampleRate"), necro_intern_string(intern, "SampleRate"), necro_intern_string(intern, "Necro.Base"), NULL);
    sample_rate_symbol->type               = necro_type_con_create(arena, sample_rate_symbol, NULL);
    sample_rate_symbol->type->kind         = base.nat_kind->type;
    sample_rate_symbol->type->pre_supplied = true;
    necro_scope_insert_ast_symbol(arena, scoped_symtable->top_type_scope, sample_rate_symbol);

    /*
        HACK: NatMul :: (Nat -> Nat -> Nat)  data type
        Uses a simple interpreter and simple Hindley-Milner type unification instead of full blown dependent typing!
    */
    NecroAstSymbol* nat_mul_symbol     = necro_ast_symbol_create(arena, necro_intern_string(intern, "Necro.Base.NatMul"), necro_intern_string(intern, "NatMul"), necro_intern_string(intern, "Necro.Base"), NULL);
    nat_mul_symbol->type               = necro_type_con_create(arena, nat_mul_symbol, NULL);
    nat_mul_symbol->type->kind         = necro_type_fn_create(arena, base.nat_kind->type, necro_type_fn_create(arena, base.nat_kind->type, base.nat_kind->type));
    nat_mul_symbol->type->pre_supplied = true;
    necro_scope_insert_ast_symbol(arena, scoped_symtable->top_type_scope, nat_mul_symbol);

    /*
        HACK: NatDiv :: (Nat -> Nat -> Nat)  data type
        Uses a simple interpreter and simple Hindley-Milner type unification instead of full blown dependent typing!
    */
    NecroAstSymbol* nat_div_symbol     = necro_ast_symbol_create(arena, necro_intern_string(intern, "Necro.Base.NatDiv"), necro_intern_string(intern, "NatDiv"), necro_intern_string(intern, "Necro.Base"), NULL);
    nat_div_symbol->type               = necro_type_con_create(arena, nat_div_symbol, NULL);
    nat_div_symbol->type->kind         = necro_type_fn_create(arena, base.nat_kind->type, necro_type_fn_create(arena, base.nat_kind->type, base.nat_kind->type));
    nat_div_symbol->type->pre_supplied = true;
    necro_scope_insert_ast_symbol(arena, scoped_symtable->top_type_scope, nat_div_symbol);

    /*
        HACK: NatMax :: (Nat -> Nat -> Nat)  data type
        Uses a simple interpreter and simple Hindley-Milner type unification instead of full blown dependent typing!
    */
    NecroAstSymbol* nat_max_symbol     = necro_ast_symbol_create(arena, necro_intern_string(intern, "Necro.Base.NatMax"), necro_intern_string(intern, "NatMax"), necro_intern_string(intern, "Necro.Base"), NULL);
    nat_max_symbol->type               = necro_type_con_create(arena, nat_max_symbol, NULL);
    nat_max_symbol->type->kind         = necro_type_fn_create(arena, base.nat_kind->type, necro_type_fn_create(arena, base.nat_kind->type, base.nat_kind->type));
    nat_max_symbol->type->pre_supplied = true;
    necro_scope_insert_ast_symbol(arena, scoped_symtable->top_type_scope, nat_max_symbol);

    /*
        HACK: NatNextPowerOfTwo :: Nat -> Nat
        Uses a simple interpreter and simple Hindley-Milner type unification instead of full blown dependent typing!
    */
    NecroAstSymbol* nat_next_power_of_2_symbol     = necro_ast_symbol_create(arena, necro_intern_string(intern, "Necro.Base.NatNextPowerOfTwo"), necro_intern_string(intern, "NatNextPowerOfTwo"), necro_intern_string(intern, "Necro.Base"), NULL);
    nat_next_power_of_2_symbol->type               = necro_type_con_create(arena, nat_next_power_of_2_symbol, NULL);
    nat_next_power_of_2_symbol->type->kind         = necro_type_fn_create(arena, base.nat_kind->type, base.nat_kind->type);
    nat_next_power_of_2_symbol->type->pre_supplied = true;
    necro_scope_insert_ast_symbol(arena, scoped_symtable->top_type_scope, nat_next_power_of_2_symbol);

    // primUndefined
    top = necro_ast_create_top_decl(arena, necro_ast_create_fn_type_sig(arena, intern, "primUndefined", NULL,
        necro_ast_create_type_attribute(arena, necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR), NECRO_TYPE_ATTRIBUTE_DOT),
        NECRO_VAR_SIG, NECRO_SIG_DECLARATION), NULL);
    base.ast.root                = top;
    NecroAst* prim_undefined_ast = necro_ast_create_simple_assignment(arena, intern, "primUndefined", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "primUndefined", NECRO_VAR_VAR), NULL));
    necro_append_top(arena, top, prim_undefined_ast);

    // ()
    NecroAst* unit_s_type            = necro_ast_create_simple_type(arena, intern, "()", NULL);
    NecroAst* unit_constructor       = necro_ast_create_data_con(arena, intern, "()", NULL);
    NecroAst* unit_constructor_list  = necro_ast_create_list(arena, unit_constructor, NULL);
    necro_append_top(arena, top, necro_ast_create_data_declaration(arena, intern, unit_s_type, unit_constructor_list));

    {
        // TODO: Make proper instance methods for (), not relying on primUndefined
        //--------------
        // Eq ()
        NecroAst* eq_method_list =
            necro_ast_create_decl(arena, necro_ast_create_prim_bin_op_method_ast(arena, intern, "eq"),
                necro_ast_create_decl(arena, necro_ast_create_prim_bin_op_method_ast(arena, intern, "neq"), NULL));
        necro_append_top(arena, top, necro_ast_create_instance(arena, intern, "Eq", necro_base_make_num_con(arena, intern, "()", false), necro_base_make_num_context(arena, intern, "Eq", false), eq_method_list));

        //--------------
        // Ord ()
        NecroAst* ord_method_list =
            necro_ast_create_decl(arena, necro_ast_create_prim_bin_op_method_ast(arena, intern, "lt"),
                necro_ast_create_decl(arena, necro_ast_create_prim_bin_op_method_ast(arena, intern, "gt"),
                    necro_ast_create_decl(arena, necro_ast_create_prim_bin_op_method_ast(arena, intern, "lte"),
                        necro_ast_create_decl(arena, necro_ast_create_prim_bin_op_method_ast(arena, intern, "gte"), NULL))));
        necro_append_top(arena, top, necro_ast_create_instance(arena, intern, "Ord", necro_base_make_num_con(arena, intern, "()", false), necro_base_make_num_context(arena, intern, "Ord", false), ord_method_list));
    }

    // []
    // NecroAst* list_s_type            = necro_ast_create_simple_type(arena, intern, "[]", necro_ast_create_var_list(arena, intern, 1, NECRO_VAR_TYPE_VAR_DECLARATION));
    // NecroAst* list_app               = necro_ast_create_type_app(arena, necro_ast_create_conid(arena, intern, "[]", NECRO_CON_TYPE_VAR), necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR));
    // NecroAst* cons_args              = necro_ast_create_list(arena, necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR), necro_ast_create_list(arena, list_app, NULL));
    // NecroAst* cons_constructor       = necro_ast_create_data_con(arena, intern, ":", cons_args);
    // NecroAst* nil_constructor        = necro_ast_create_data_con(arena, intern, "[]", NULL);
    // NecroAst* list_constructor_list  = necro_ast_create_list(arena, cons_constructor, necro_ast_create_list(arena, nil_constructor, NULL));
    // necro_append_top(arena, top, necro_ast_create_data_declaration(arena, intern, list_s_type, list_constructor_list));

    // Simple Data Decls
    necro_base_create_simple_data_decl(arena, top, intern, "World", "_World");
    necro_base_create_simple_data_decl(arena, top, intern, "Int", "_Int");
    necro_base_create_simple_data_decl(arena, top, intern, "UInt", "_UInt");
    necro_base_create_simple_data_decl(arena, top, intern, "Float", "_Float");
    necro_base_create_simple_data_decl(arena, top, intern, "Char", "_Char");
    // necro_base_create_simple_data_decl(arena, top, intern, "F64", "_F64");
    // necro_base_create_simple_data_decl(arena, top, intern, "I64", "_I64");

    // FloatVec
    NecroAst* float_vec_n_type   = necro_ast_create_type_signature(arena, NECRO_SIG_TYPE_VAR, necro_ast_create_var(arena, intern, "n", NECRO_VAR_TYPE_VAR_DECLARATION), NULL, necro_ast_create_conid(arena, intern, "Nat", NECRO_CON_TYPE_VAR));
    NecroAst* float_vec_s_type   = necro_ast_create_simple_type(arena, intern, "FloatVec", necro_ast_create_list(arena, float_vec_n_type, NULL));
    NecroAst* float_vec_con      = necro_ast_create_data_con(arena, intern, "_FloatVec", NULL);
    NecroAst* float_vec_con_list = necro_ast_create_list(arena, float_vec_con, NULL);
    necro_append_top(arena, top, necro_ast_create_data_declaration(arena, intern, float_vec_s_type, float_vec_con_list));

    // _project :: a -> UInt -> b, primitive data structure projection
    {
        necro_append_top(arena, top,
            necro_ast_create_fn_type_sig(arena, intern, "_project", NULL,
                necro_ast_create_type_fn(arena,
                    necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                    necro_ast_create_type_fn(arena,
                        necro_ast_create_conid(arena, intern, "Int", NECRO_CON_TYPE_VAR),
                        necro_ast_create_var(arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR))),
                NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        necro_append_top(arena, top,
            necro_ast_create_apats_assignment(arena, intern, "_project",
                necro_ast_create_apats(arena, necro_ast_create_var(arena, intern, "con", NECRO_VAR_DECLARATION), necro_ast_create_apats(arena, necro_ast_create_var(arena, intern, "slot", NECRO_VAR_DECLARATION), NULL)),
                necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "primUndefined", NECRO_VAR_VAR), NULL)));
    }

    // Simple Poly Data Decls
    necro_base_create_simple_poly_data_decl(arena, top, intern, "Ptr");

    // Array
    NecroAst* array_n_type   = necro_ast_create_type_signature(arena, NECRO_SIG_TYPE_VAR, necro_ast_create_var(arena, intern, "n", NECRO_VAR_TYPE_VAR_DECLARATION), NULL, necro_ast_create_conid(arena, intern, "Nat", NECRO_CON_TYPE_VAR));
    NecroAst* array_s_type   = necro_ast_create_simple_type(arena, intern, "Array", necro_ast_create_list(arena, array_n_type, necro_ast_create_var_list(arena, intern, 1, NECRO_VAR_TYPE_VAR_DECLARATION)));
    NecroAst* array_args     = necro_ast_create_list(arena, necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR), NULL);
    NecroAst* array_con      = necro_ast_create_data_con(arena, intern, "_Array", array_args);
    NecroAst* array_con_list = necro_ast_create_list(arena, array_con, NULL);
    necro_append_top(arena, top, necro_ast_create_data_declaration(arena, intern, array_s_type, array_con_list));

    //--------------------
    // Classes
    //--------------------

    // (,)
    NecroAst* tuple_2_s_type      = necro_ast_create_simple_type(arena, intern, "(,)", necro_ast_create_var_list(arena, intern, 2, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroAst* tuple_2_constructor = necro_ast_create_data_con(arena, intern, "(,)", necro_ast_create_var_list(arena, intern, 2, NECRO_VAR_TYPE_FREE_VAR));
    necro_append_top(arena, top, necro_ast_create_data_declaration(arena, intern, tuple_2_s_type, necro_ast_create_list(arena, tuple_2_constructor, NULL)));

    // (,,)
    NecroAst* tuple_3_s_type      = necro_ast_create_simple_type(arena, intern, "(,,)", necro_ast_create_var_list(arena, intern, 3, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroAst* tuple_3_constructor = necro_ast_create_data_con(arena, intern, "(,,)", necro_ast_create_var_list(arena, intern, 3, NECRO_VAR_TYPE_FREE_VAR));
    necro_append_top(arena, top, necro_ast_create_data_declaration(arena, intern, tuple_3_s_type, necro_ast_create_list(arena, tuple_3_constructor, NULL)));

    // (,,,)
    NecroAst* tuple_4_s_type      = necro_ast_create_simple_type(arena, intern, "(,,,)", necro_ast_create_var_list(arena, intern, 4, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroAst* tuple_4_constructor = necro_ast_create_data_con(arena, intern, "(,,,)", necro_ast_create_var_list(arena, intern, 4, NECRO_VAR_TYPE_FREE_VAR));
    necro_append_top(arena, top, necro_ast_create_data_declaration(arena, intern, tuple_4_s_type, necro_ast_create_list(arena, tuple_4_constructor, NULL)));

    // (,,,,)
    NecroAst* tuple_5_s_type      = necro_ast_create_simple_type(arena, intern, "(,,,,)", necro_ast_create_var_list(arena, intern, 5, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroAst* tuple_5_constructor = necro_ast_create_data_con(arena, intern, "(,,,,)", necro_ast_create_var_list(arena, intern, 5, NECRO_VAR_TYPE_FREE_VAR));
    necro_append_top(arena, top, necro_ast_create_data_declaration(arena, intern, tuple_5_s_type, necro_ast_create_list(arena, tuple_5_constructor, NULL)));

    // (,,,,,)
    NecroAst* tuple_6_s_type      = necro_ast_create_simple_type(arena, intern, "(,,,,,)", necro_ast_create_var_list(arena, intern, 6, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroAst* tuple_6_constructor = necro_ast_create_data_con(arena, intern, "(,,,,,)", necro_ast_create_var_list(arena, intern, 6, NECRO_VAR_TYPE_FREE_VAR));
    necro_append_top(arena, top, necro_ast_create_data_declaration(arena, intern, tuple_6_s_type, necro_ast_create_list(arena, tuple_6_constructor, NULL)));

    // (,,,,,,)
    NecroAst* tuple_7_s_type      = necro_ast_create_simple_type(arena, intern, "(,,,,,,)", necro_ast_create_var_list(arena, intern, 7, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroAst* tuple_7_constructor = necro_ast_create_data_con(arena, intern, "(,,,,,,)", necro_ast_create_var_list(arena, intern, 7, NECRO_VAR_TYPE_FREE_VAR));
    necro_append_top(arena, top, necro_ast_create_data_declaration(arena, intern, tuple_7_s_type, necro_ast_create_list(arena, tuple_7_constructor, NULL)));

    // (,,,,,,,)
    NecroAst* tuple_8_s_type      = necro_ast_create_simple_type(arena, intern, "(,,,,,,,)", necro_ast_create_var_list(arena, intern, 8, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroAst* tuple_8_constructor = necro_ast_create_data_con(arena, intern, "(,,,,,,,)", necro_ast_create_var_list(arena, intern, 8, NECRO_VAR_TYPE_FREE_VAR));
    necro_append_top(arena, top, necro_ast_create_data_declaration(arena, intern, tuple_8_s_type, necro_ast_create_list(arena, tuple_8_constructor, NULL)));

    // (,,,,,,,,)
    NecroAst* tuple_9_s_type      = necro_ast_create_simple_type(arena, intern, "(,,,,,,,,)", necro_ast_create_var_list(arena, intern, 9, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroAst* tuple_9_constructor = necro_ast_create_data_con(arena, intern, "(,,,,,,,,)", necro_ast_create_var_list(arena, intern, 9, NECRO_VAR_TYPE_FREE_VAR));
    necro_append_top(arena, top, necro_ast_create_data_declaration(arena, intern, tuple_9_s_type, necro_ast_create_list(arena, tuple_9_constructor, NULL)));

    // (,,,,,,,,,)
    NecroAst* tuple_10_s_type      = necro_ast_create_simple_type(arena, intern, "(,,,,,,,,,)", necro_ast_create_var_list(arena, intern, 10, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroAst* tuple_10_constructor = necro_ast_create_data_con(arena, intern, "(,,,,,,,,,)", necro_ast_create_var_list(arena, intern, 10, NECRO_VAR_TYPE_FREE_VAR));
    necro_append_top(arena, top, necro_ast_create_data_declaration(arena, intern, tuple_10_s_type, necro_ast_create_list(arena, tuple_10_constructor, NULL)));

    for (size_t i = 2; i <= NECRO_MAX_UNBOXED_TUPLE_TYPES; ++i)
    {
        char name[16] = "";
        snprintf(name, 16, "(#,#)%zu", i);
        NecroAst* unboxed_tuple_s_type      = necro_ast_create_simple_type(arena, intern, name, necro_ast_create_var_list(arena, intern, i, NECRO_VAR_TYPE_VAR_DECLARATION));
        NecroAst* unboxed_tuple_constructor = necro_ast_create_data_con(arena, intern, name, necro_ast_create_var_list(arena, intern, i, NECRO_VAR_TYPE_FREE_VAR));
        necro_append_top(arena, top, necro_ast_create_data_declaration(arena, intern, unboxed_tuple_s_type, necro_ast_create_list(arena, unboxed_tuple_constructor, NULL)));
    }

    for (size_t i = 0; i < NECRO_MAX_ENV_TYPES; ++i)
    {
        char env_name[16] = "";
        snprintf(env_name, 16, "Env%zu", i);
        NecroAst* env_s_type      = necro_ast_create_simple_type(arena, intern, env_name, necro_ast_create_var_list(arena, intern, i, NECRO_VAR_TYPE_VAR_DECLARATION));
        NecroAst* env_constructor = necro_ast_create_data_con(arena, intern, env_name, necro_ast_create_var_list(arena, intern, i, NECRO_VAR_TYPE_FREE_VAR));
        necro_append_top(arena, top, necro_ast_create_data_declaration(arena, intern, env_s_type, necro_ast_create_list(arena, env_constructor, NULL)));
    }

    // for (size_t i = 2; i < NECRO_MAX_BRANCH_TYPES; ++i)
    // {
    //     char branch_fn_name[24] = "";
    //     snprintf(branch_fn_name, 24, "BranchFn%zu", i);
    //     NecroAst* branch_fn_s_type = necro_ast_create_simple_type(arena, intern, branch_fn_name, necro_ast_create_var_list(arena, intern, i, NECRO_VAR_TYPE_VAR_DECLARATION));
    //     NecroAst* branch_fn_cons   = NULL;
    //     for (size_t i2 = 0; i2 < i; ++i2)
    //     {
    //         snprintf(branch_fn_name, 24, "BranchFn%zuAlt%zu", i, i2);
    //         NecroAst* branch_fn_con_var     = necro_ast_create_var(arena, intern, var_names[i2], NECRO_VAR_TYPE_FREE_VAR);
    //         NecroAst* branch_fn_constructor = necro_ast_create_data_con(arena, intern, branch_fn_name, necro_ast_create_list(arena, branch_fn_con_var, NULL));
    //         branch_fn_cons                  = necro_ast_create_list(arena, branch_fn_constructor, branch_fn_cons);
    //     }
    //     necro_append_top(arena, top, necro_ast_create_data_declaration(arena, intern, branch_fn_s_type, branch_fn_cons));
    // }

    //--------------------
    // BinOps
    //--------------------

    // ==
    necro_append_top(arena, top, necro_base_create_eq_comp_sig(arena, intern, "=="));
    necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "==", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "eq", NECRO_VAR_VAR), NULL)));

    // /=
    necro_append_top(arena, top,  necro_base_create_eq_comp_sig(arena, intern, "/="));
    necro_append_top(arena, top,  necro_ast_create_simple_assignment(arena, intern, "/=", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "neq", NECRO_VAR_VAR), NULL)));

    // <
    necro_append_top(arena, top, necro_base_create_ord_comp_sig(arena, intern, "<"));
    necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "<", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "lt", NECRO_VAR_VAR), NULL)));

    // >
    necro_append_top(arena, top, necro_base_create_ord_comp_sig(arena, intern, ">"));
    necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, ">", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "gt", NECRO_VAR_VAR), NULL)));

    // <=
    necro_append_top(arena, top, necro_base_create_ord_comp_sig(arena, intern, "<="));
    necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "<=", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "lte", NECRO_VAR_VAR), NULL)));

    // >=
    necro_append_top(arena, top, necro_base_create_ord_comp_sig(arena, intern, ">="));
    necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, ">=", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "gte", NECRO_VAR_VAR), NULL)));

    // +
    {
        NecroAst* a_var = necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* op_sig =
            necro_ast_create_fn_type_sig(arena, intern, "+", necro_ast_create_context(arena, intern, "Semiring", "a", NULL),
                necro_ast_create_type_fn(arena, a_var, necro_ast_create_type_fn(arena, a_var, a_var)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        necro_append_top(arena, top, op_sig);
        necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "+", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "add", NECRO_VAR_VAR), NULL)));
    }

    // *
    {
        NecroAst* a_var = necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* op_sig =
            necro_ast_create_fn_type_sig(arena, intern, "*", necro_ast_create_context(arena, intern, "Semiring", "a", NULL),
                necro_ast_create_type_fn(arena, a_var, necro_ast_create_type_fn(arena, a_var, a_var)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        necro_append_top(arena, top, op_sig);
        necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "*", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "mul", NECRO_VAR_VAR), NULL)));
    }

    // -
    {
        NecroAst* a_var = necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* op_sig =
            necro_ast_create_fn_type_sig(arena, intern, "-", necro_ast_create_context(arena, intern, "Ring", "a", NULL),
                necro_ast_create_type_fn(arena, a_var, necro_ast_create_type_fn(arena, a_var, a_var)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        necro_append_top(arena, top, op_sig);
        necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "-", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "sub", NECRO_VAR_VAR), NULL)));
    }

    // /
    {
        NecroAst* a_var = necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* op_sig =
            necro_ast_create_fn_type_sig(arena, intern, "/", necro_ast_create_context(arena, intern, "EuclideanRing", "a", NULL),
                necro_ast_create_type_fn(arena, a_var, necro_ast_create_type_fn(arena, a_var, a_var)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        necro_append_top(arena, top, op_sig);
        necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "/", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "div", NECRO_VAR_VAR), NULL)));
    }

    // %
    {
        NecroAst* a_var  = necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* op_sig =
            necro_ast_create_fn_type_sig(arena, intern, "%", necro_ast_create_context(arena, intern, "EuclideanRing", "a", NULL),
                necro_ast_create_type_fn(arena, a_var, necro_ast_create_type_fn(arena, a_var, a_var)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        necro_append_top(arena, top, op_sig);
        necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "%", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "rem", NECRO_VAR_VAR), NULL)));
    }

    // <>
    {
        NecroAst* a_var  = necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* op_sig =
            necro_ast_create_fn_type_sig(arena, intern, "<>", necro_ast_create_context(arena, intern, "Semigroup", "a", NULL),
                necro_ast_create_type_fn(arena, a_var, necro_ast_create_type_fn(arena, a_var, a_var)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        necro_append_top(arena, top, op_sig);
        necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "<>", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "append", NECRO_VAR_VAR), NULL)));
    }

    // @+
    {
        NecroAst* seq_type = necro_ast_create_type_app(arena, necro_ast_create_conid(arena, intern, "Seq", NECRO_CON_TYPE_VAR), necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR));
        necro_append_top(arena, top, necro_ast_create_fn_type_sig(arena, intern, "@+", necro_ast_create_context(arena, intern, "Num", "a", NULL), necro_ast_create_type_fn(arena, seq_type, necro_ast_create_type_fn(arena, seq_type, seq_type)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "@+", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "addSeqOnLeft", NECRO_VAR_VAR), NULL)));
    }

    // +@
    {
        NecroAst* seq_type = necro_ast_create_type_app(arena, necro_ast_create_conid(arena, intern, "Seq", NECRO_CON_TYPE_VAR), necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR));
        necro_append_top(arena, top, necro_ast_create_fn_type_sig(arena, intern, "+@", necro_ast_create_context(arena, intern, "Num", "a", NULL), necro_ast_create_type_fn(arena, seq_type, necro_ast_create_type_fn(arena, seq_type, seq_type)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "+@", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "addSeqOnRight", NECRO_VAR_VAR), NULL)));
    }

    // @-
    {
        NecroAst* seq_type = necro_ast_create_type_app(arena, necro_ast_create_conid(arena, intern, "Seq", NECRO_CON_TYPE_VAR), necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR));
        necro_append_top(arena, top, necro_ast_create_fn_type_sig(arena, intern, "@-", necro_ast_create_context(arena, intern, "Num", "a", NULL), necro_ast_create_type_fn(arena, seq_type, necro_ast_create_type_fn(arena, seq_type, seq_type)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "@-", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "subSeqOnLeft", NECRO_VAR_VAR), NULL)));
    }

    // -@
    {
        NecroAst* seq_type = necro_ast_create_type_app(arena, necro_ast_create_conid(arena, intern, "Seq", NECRO_CON_TYPE_VAR), necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR));
        necro_append_top(arena, top, necro_ast_create_fn_type_sig(arena, intern, "-@", necro_ast_create_context(arena, intern, "Num", "a", NULL), necro_ast_create_type_fn(arena, seq_type, necro_ast_create_type_fn(arena, seq_type, seq_type)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "-@", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "subSeqOnRight", NECRO_VAR_VAR), NULL)));
    }

    // @*
    {
        NecroAst* seq_type = necro_ast_create_type_app(arena, necro_ast_create_conid(arena, intern, "Seq", NECRO_CON_TYPE_VAR), necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR));
        necro_append_top(arena, top, necro_ast_create_fn_type_sig(arena, intern, "@*", necro_ast_create_context(arena, intern, "Num", "a", NULL), necro_ast_create_type_fn(arena, seq_type, necro_ast_create_type_fn(arena, seq_type, seq_type)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "@*", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "mulSeqOnLeft", NECRO_VAR_VAR), NULL)));
    }

    // *@
    {
        NecroAst* seq_type = necro_ast_create_type_app(arena, necro_ast_create_conid(arena, intern, "Seq", NECRO_CON_TYPE_VAR), necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR));
        necro_append_top(arena, top, necro_ast_create_fn_type_sig(arena, intern, "*@", necro_ast_create_context(arena, intern, "Num", "a", NULL), necro_ast_create_type_fn(arena, seq_type, necro_ast_create_type_fn(arena, seq_type, seq_type)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "*@", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "mulSeqOnRight", NECRO_VAR_VAR), NULL)));
    }

    // @/
    {
        NecroAst* seq_type = necro_ast_create_type_app(arena, necro_ast_create_conid(arena, intern, "Seq", NECRO_CON_TYPE_VAR), necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR));
        necro_append_top(arena, top, necro_ast_create_fn_type_sig(arena, intern, "@/", necro_ast_create_context(arena, intern, "EuclideanRing", "a", NULL), necro_ast_create_type_fn(arena, seq_type, necro_ast_create_type_fn(arena, seq_type, seq_type)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "@/", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "divSeqOnLeft", NECRO_VAR_VAR), NULL)));
    }

    // /@
    {
        NecroAst* seq_type = necro_ast_create_type_app(arena, necro_ast_create_conid(arena, intern, "Seq", NECRO_CON_TYPE_VAR), necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR));
        necro_append_top(arena, top, necro_ast_create_fn_type_sig(arena, intern, "/@", necro_ast_create_context(arena, intern, "EuclideanRing", "a", NULL), necro_ast_create_type_fn(arena, seq_type, necro_ast_create_type_fn(arena, seq_type, seq_type)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "/@", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "divSeqOnRight", NECRO_VAR_VAR), NULL)));
    }

    // @<
    {
        NecroAst* seq_type_a = necro_ast_create_type_app(arena, necro_ast_create_conid(arena, intern, "Seq", NECRO_CON_TYPE_VAR), necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR));
        NecroAst* seq_type_b = necro_ast_create_type_app(arena, necro_ast_create_conid(arena, intern, "Seq", NECRO_CON_TYPE_VAR), necro_ast_create_var(arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR));
        necro_append_top(arena, top, necro_ast_create_fn_type_sig(arena, intern, "@<", NULL, necro_ast_create_type_fn(arena, seq_type_a, necro_ast_create_type_fn(arena, seq_type_b, seq_type_a)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "@<", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "leftConstSeqOnLeft", NECRO_VAR_VAR), NULL)));
    }

    // <@
    {
        NecroAst* seq_type_a = necro_ast_create_type_app(arena, necro_ast_create_conid(arena, intern, "Seq", NECRO_CON_TYPE_VAR), necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR));
        NecroAst* seq_type_b = necro_ast_create_type_app(arena, necro_ast_create_conid(arena, intern, "Seq", NECRO_CON_TYPE_VAR), necro_ast_create_var(arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR));
        necro_append_top(arena, top, necro_ast_create_fn_type_sig(arena, intern, "<@", NULL, necro_ast_create_type_fn(arena, seq_type_a, necro_ast_create_type_fn(arena, seq_type_b, seq_type_a)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "<@", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "leftConstSeqOnRight", NECRO_VAR_VAR), NULL)));
    }

    // @>
    {
        NecroAst* seq_type_a = necro_ast_create_type_app(arena, necro_ast_create_conid(arena, intern, "Seq", NECRO_CON_TYPE_VAR), necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR));
        NecroAst* seq_type_b = necro_ast_create_type_app(arena, necro_ast_create_conid(arena, intern, "Seq", NECRO_CON_TYPE_VAR), necro_ast_create_var(arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR));
        necro_append_top(arena, top, necro_ast_create_fn_type_sig(arena, intern, "@>", NULL, necro_ast_create_type_fn(arena, seq_type_a, necro_ast_create_type_fn(arena, seq_type_b, seq_type_b)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "@>", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "rightConstSeqOnLeft", NECRO_VAR_VAR), NULL)));
    }

    // >@
    {
        NecroAst* seq_type_a = necro_ast_create_type_app(arena, necro_ast_create_conid(arena, intern, "Seq", NECRO_CON_TYPE_VAR), necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR));
        NecroAst* seq_type_b = necro_ast_create_type_app(arena, necro_ast_create_conid(arena, intern, "Seq", NECRO_CON_TYPE_VAR), necro_ast_create_var(arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR));
        necro_append_top(arena, top, necro_ast_create_fn_type_sig(arena, intern, ">@", NULL, necro_ast_create_type_fn(arena, seq_type_a, necro_ast_create_type_fn(arena, seq_type_b, seq_type_b)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, ">@", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "rightConstSeqOnRight", NECRO_VAR_VAR), NULL)));
    }

    // @<@
    {
        NecroAst* seq_type_a = necro_ast_create_type_app(arena, necro_ast_create_conid(arena, intern, "Seq", NECRO_CON_TYPE_VAR), necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR));
        NecroAst* seq_type_b = necro_ast_create_type_app(arena, necro_ast_create_conid(arena, intern, "Seq", NECRO_CON_TYPE_VAR), necro_ast_create_var(arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR));
        necro_append_top(arena, top, necro_ast_create_fn_type_sig(arena, intern, "@<@", NULL, necro_ast_create_type_fn(arena, seq_type_a, necro_ast_create_type_fn(arena, seq_type_b, seq_type_a)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "@<@", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "leftConstSeqOnBoth", NECRO_VAR_VAR), NULL)));
    }

    // @>@
    {
        NecroAst* seq_type_a = necro_ast_create_type_app(arena, necro_ast_create_conid(arena, intern, "Seq", NECRO_CON_TYPE_VAR), necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR));
        NecroAst* seq_type_b = necro_ast_create_type_app(arena, necro_ast_create_conid(arena, intern, "Seq", NECRO_CON_TYPE_VAR), necro_ast_create_var(arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR));
        necro_append_top(arena, top, necro_ast_create_fn_type_sig(arena, intern, "@>@", NULL, necro_ast_create_type_fn(arena, seq_type_a, necro_ast_create_type_fn(arena, seq_type_b, seq_type_b)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "@>@", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "rightConstSeqOnBoth", NECRO_VAR_VAR), NULL)));
    }

    // // >>=
    // {
    //     NecroAst* a_var       = necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
    //     NecroAst* b_var       = necro_ast_create_var(arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
    //     NecroAst* m_var       = necro_ast_create_var(arena, intern, "m", NECRO_VAR_TYPE_FREE_VAR);
    //     NecroAst* bind_op_sig =
    //         necro_ast_create_fn_type_sig(arena, intern, ">>=", necro_ast_create_context(arena, intern, "Monad", "m", NULL),
    //             necro_ast_create_type_fn(arena,
    //                 necro_ast_create_type_app(arena, m_var, a_var),
    //                 necro_ast_create_type_fn(arena,
    //                     necro_ast_create_type_fn(arena, a_var, necro_ast_create_type_app(arena, m_var, b_var)),
    //                     necro_ast_create_type_app(arena, m_var, b_var))),
    //             NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
    //     necro_append_top(arena, top, bind_op_sig);
    //     necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, ">>=", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "bind", NECRO_VAR_VAR), NULL)));
    // }

    // >> , no longer monadic bind operator, co-opted by forwards compose operator
    // {
    //     NecroAst* a_var       = necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
    //     NecroAst* b_var       = necro_ast_create_var(arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
    //     NecroAst* m_var       = necro_ast_create_var(arena, intern, "m", NECRO_VAR_TYPE_FREE_VAR);
    //     NecroAst* op_sig =
    //         necro_ast_create_fn_type_sig(arena, intern, ">>", necro_ast_create_context(arena, intern, "Monad", "m", NULL),
    //             necro_ast_create_type_fn(arena,
    //                 necro_ast_create_type_app(arena, m_var, a_var),
    //                 necro_ast_create_type_fn(arena,
    //                     necro_ast_create_type_app(arena, m_var, b_var),
    //                     necro_ast_create_type_app(arena, m_var, b_var))),
    //             NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
    //     NecroAst* mm_var      = necro_ast_create_var(arena, intern, "m", NECRO_VAR_DECLARATION);
    //     NecroAst* mk_var      = necro_ast_create_var(arena, intern, "k", NECRO_VAR_DECLARATION);
    //     NecroAst* op_args     = necro_ast_create_apats(arena, mm_var, necro_ast_create_apats(arena, mk_var, NULL));
    //     NecroAst* bind        = necro_ast_create_fexpr(arena,
    //         necro_ast_create_fexpr(arena, necro_ast_create_var(arena, intern, "bind", NECRO_VAR_VAR), necro_ast_create_var(arena, intern, "m", NECRO_VAR_VAR)),
    //         necro_ast_create_lambda(arena, necro_ast_create_apats(arena, necro_ast_create_wildcard(arena), NULL), necro_ast_create_var(arena, intern, "k", NECRO_VAR_VAR)));
    //     NecroAst* op_rhs      = necro_ast_create_rhs(arena, bind, NULL);
    //     NecroAst* op_def_ast  = necro_ast_create_apats_assignment(arena, intern, ">>", op_args, op_rhs);
    //     necro_append_top(arena, top, op_sig);
    //     necro_append_top(arena, top, op_def_ast);
    // }

    // <| :: .(.a -> .b) -> .(.a -> .b)
    {
        NecroAst* a_var       = necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* b_var       = necro_ast_create_var(arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        a_var->variable.order = NECRO_TYPE_HIGHER_ORDER;
        b_var->variable.order = NECRO_TYPE_HIGHER_ORDER;
        NecroAst* op_sig      =
            necro_ast_create_fn_type_sig(arena, intern, "<|", NULL,
                necro_ast_create_type_fn(arena,
                    necro_ast_create_type_attribute(arena, necro_ast_create_type_fn(arena, necro_ast_create_type_attribute(arena, a_var, NECRO_TYPE_ATTRIBUTE_DOT), necro_ast_create_type_attribute(arena, b_var, NECRO_TYPE_ATTRIBUTE_DOT)), NECRO_TYPE_ATTRIBUTE_DOT),
                    necro_ast_create_type_attribute(arena, necro_ast_create_type_fn(arena, necro_ast_create_type_attribute(arena, a_var, NECRO_TYPE_ATTRIBUTE_DOT), necro_ast_create_type_attribute(arena, b_var, NECRO_TYPE_ATTRIBUTE_DOT)), NECRO_TYPE_ATTRIBUTE_DOT)),
                NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        NecroAst* f_var       = necro_ast_create_var(arena, intern, "f", NECRO_VAR_DECLARATION);
        NecroAst* x_var       = necro_ast_create_var(arena, intern, "x", NECRO_VAR_DECLARATION);
        NecroAst* op_args     = necro_ast_create_apats(arena, f_var, necro_ast_create_apats(arena, x_var, NULL));
        NecroAst* op_rhs_ast  = necro_ast_create_rhs(arena, necro_ast_create_fexpr(arena, necro_ast_create_var(arena, intern, "f", NECRO_VAR_VAR), necro_ast_create_var(arena, intern, "x", NECRO_VAR_VAR)), NULL);
        NecroAst* op_def_ast  = necro_ast_create_apats_assignment(arena, intern, "<|", op_args, op_rhs_ast);
        necro_append_top(arena, top, op_sig);
        necro_append_top(arena, top, op_def_ast);
    }

    // |>
    {
        NecroAst* a_var       = necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* b_var       = necro_ast_create_var(arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        a_var->variable.order = NECRO_TYPE_HIGHER_ORDER;
        b_var->variable.order = NECRO_TYPE_HIGHER_ORDER;
        NecroAst* op_sig      =
            necro_ast_create_fn_type_sig(arena, intern, "|>", NULL,
                necro_ast_create_type_fn(arena,
                    necro_ast_create_type_attribute(arena, a_var, NECRO_TYPE_ATTRIBUTE_DOT),
                        necro_ast_create_type_fn(arena,
                            necro_ast_create_type_attribute(arena, necro_ast_create_type_fn(arena,
                                necro_ast_create_type_attribute(arena, a_var, NECRO_TYPE_ATTRIBUTE_DOT),
                                necro_ast_create_type_attribute(arena, b_var, NECRO_TYPE_ATTRIBUTE_DOT)),
                                NECRO_TYPE_ATTRIBUTE_DOT),
                            necro_ast_create_type_attribute(arena, b_var, NECRO_TYPE_ATTRIBUTE_DOT))),
                NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        NecroAst* x_var       = necro_ast_create_var(arena, intern, "x", NECRO_VAR_DECLARATION);
        NecroAst* f_var       = necro_ast_create_var(arena, intern, "f", NECRO_VAR_DECLARATION);
        NecroAst* op_args     = necro_ast_create_apats(arena, x_var, necro_ast_create_apats(arena, f_var, NULL));
        NecroAst* op_rhs_ast  = necro_ast_create_rhs(arena, necro_ast_create_fexpr(arena, necro_ast_create_var(arena, intern, "f", NECRO_VAR_VAR), necro_ast_create_var(arena, intern, "x", NECRO_VAR_VAR)), NULL);
        NecroAst* op_def_ast  = necro_ast_create_apats_assignment(arena, intern, "|>", op_args, op_rhs_ast);
        necro_append_top(arena, top, op_sig);
        necro_append_top(arena, top, op_def_ast);
    }

    // <. :: (b -> c) -> (a -> b) -> a -> c
    {
        NecroAst* a_var       = necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* b_var       = necro_ast_create_var(arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* c_var       = necro_ast_create_var(arena, intern, "c", NECRO_VAR_TYPE_FREE_VAR);
        a_var->variable.order = NECRO_TYPE_HIGHER_ORDER;
        b_var->variable.order = NECRO_TYPE_HIGHER_ORDER;
        c_var->variable.order = NECRO_TYPE_HIGHER_ORDER;
        NecroAst* op_sig      =
            necro_ast_create_fn_type_sig(arena, intern, "<.", NULL,
                necro_ast_create_type_fn(arena, necro_ast_create_type_fn(arena, b_var, c_var),
                    necro_ast_create_type_fn(arena, necro_ast_create_type_fn(arena, a_var, b_var),
                        necro_ast_create_type_fn(arena, a_var, c_var))),
                NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        NecroAst* f_var       = necro_ast_create_var(arena, intern, "f", NECRO_VAR_DECLARATION);
        NecroAst* g_var       = necro_ast_create_var(arena, intern, "g", NECRO_VAR_DECLARATION);
        NecroAst* x_var       = necro_ast_create_var(arena, intern, "x", NECRO_VAR_DECLARATION);
        NecroAst* op_args     = necro_ast_create_apats(arena, f_var, necro_ast_create_apats(arena, g_var, NULL));
        NecroAst* op_lambda   = necro_ast_create_lambda(arena, necro_ast_create_apats(arena, x_var, NULL),
            necro_ast_create_fexpr(arena, necro_ast_create_var(arena, intern, "f", NECRO_VAR_VAR),
                necro_ast_create_fexpr(arena, necro_ast_create_var(arena, intern, "g", NECRO_VAR_VAR),
                    necro_ast_create_var(arena, intern, "x", NECRO_VAR_VAR))));
        NecroAst* op_rhs_ast  = necro_ast_create_rhs(arena, op_lambda, NULL);
        NecroAst* op_def_ast  = necro_ast_create_apats_assignment(arena, intern, "<.", op_args, op_rhs_ast);
        necro_append_top(arena, top, op_sig);
        necro_append_top(arena, top, op_def_ast);
    }

    // .> :: (a -> b) -> (b -> c) -> a -> c
    {
        NecroAst* a_var       = necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* b_var       = necro_ast_create_var(arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* c_var       = necro_ast_create_var(arena, intern, "c", NECRO_VAR_TYPE_FREE_VAR);
        a_var->variable.order = NECRO_TYPE_HIGHER_ORDER;
        b_var->variable.order = NECRO_TYPE_HIGHER_ORDER;
        c_var->variable.order = NECRO_TYPE_HIGHER_ORDER;
        NecroAst* op_sig      =
            necro_ast_create_fn_type_sig(arena, intern, ".>", NULL,
                necro_ast_create_type_fn(arena, necro_ast_create_type_fn(arena, a_var, b_var),
                    necro_ast_create_type_fn(arena, necro_ast_create_type_fn(arena, b_var, c_var),
                        necro_ast_create_type_fn(arena, a_var, c_var))),
                NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        NecroAst* f_var       = necro_ast_create_var(arena, intern, "f", NECRO_VAR_DECLARATION);
        NecroAst* g_var       = necro_ast_create_var(arena, intern, "g", NECRO_VAR_DECLARATION);
        NecroAst* x_var       = necro_ast_create_var(arena, intern, "x", NECRO_VAR_DECLARATION);
        NecroAst* op_args     = necro_ast_create_apats(arena, f_var, necro_ast_create_apats(arena, g_var, NULL));
        NecroAst* op_lambda   = necro_ast_create_lambda(arena, necro_ast_create_apats(arena, x_var, NULL),
            necro_ast_create_fexpr(arena, necro_ast_create_var(arena, intern, "g", NECRO_VAR_VAR),
                necro_ast_create_fexpr(arena, necro_ast_create_var(arena, intern, "f", NECRO_VAR_VAR),
                    necro_ast_create_var(arena, intern, "x", NECRO_VAR_VAR))));
        NecroAst* op_rhs_ast  = necro_ast_create_rhs(arena, op_lambda, NULL);
        NecroAst* op_def_ast  = necro_ast_create_apats_assignment(arena, intern, ".>", op_args, op_rhs_ast);
        necro_append_top(arena, top, op_sig);
        necro_append_top(arena, top, op_def_ast);
    }

    // TODO: Look into this. Looks like somewhere along the way we broke polymorphic pat assignments.
    // // prev
    // {
    //     NecroAst* a_var       = necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
    //     a_var->variable.order = NECRO_TYPE_ZERO_ORDER;
    //     NecroAst* context     = necro_ast_create_context(arena, intern, "Default", "a", NULL);
    //     NecroAst* fn_sig      =
    //         necro_ast_create_fn_type_sig(arena, intern, "prev", context,
    //             necro_ast_create_type_fn(arena, a_var, a_var),
    //             NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
    //     NecroAst* x_var       = necro_ast_create_var(arena, intern, "x", NECRO_VAR_DECLARATION);
    //     NecroAst* fn_args     = necro_ast_create_apats(arena, x_var, NULL);
    //     NecroAst* px_var      = necro_ast_create_var(arena, intern, "px", NECRO_VAR_DECLARATION);
    //     NecroAst* cx_var      = necro_ast_create_var(arena, intern, "cx", NECRO_VAR_DECLARATION);
    //     px_var->variable.initializer = necro_ast_create_var(arena, intern, "default", NECRO_VAR_VAR);
    //     NecroAst* pat_assign  = necro_ast_create_pat_assignment(arena,
    //         necro_ast_create_tuple(arena, necro_ast_create_list(arena, px_var, necro_ast_create_list(arena, cx_var, NULL))),
    //         necro_ast_create_tuple(arena, necro_ast_create_list(arena, necro_ast_create_var(arena, intern, "x", NECRO_VAR_VAR), necro_ast_create_list(arena, necro_ast_create_var(arena, intern, "px", NECRO_VAR_VAR), NULL))));
    //     NecroAst* fn_decl     = necro_ast_create_decl(arena, pat_assign, NULL);
    //     NecroAst* fn_rhs_ast  = necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "cx", NECRO_VAR_VAR), fn_decl);
    //     NecroAst* fn_def_ast  = necro_ast_create_apats_assignment(arena, intern, "prev", fn_args, fn_rhs_ast);
    //     necro_append_top(arena, top, fn_sig);
    //     necro_append_top(arena, top, fn_def_ast);
    // }

    // getMouseX
    {
        necro_append_top(arena, top, necro_ast_create_fn_type_sig(arena, intern, "getMouseX", NULL,
            necro_ast_create_type_fn(arena,
                necro_ast_create_conid(arena, intern, "()", NECRO_CON_TYPE_VAR),
                necro_ast_create_conid(arena, intern, "Int", NECRO_CON_TYPE_VAR)),
            NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        necro_append_top(arena, top,
            necro_ast_create_apats_assignment(arena, intern, "getMouseX",
                necro_ast_create_apats(arena, necro_ast_create_var(arena, intern, "_dummy", NECRO_VAR_DECLARATION), NULL),
                necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "primUndefined", NECRO_VAR_VAR), NULL)));
    }

    // getMouseY
    {
        necro_append_top(arena, top, necro_ast_create_fn_type_sig(arena, intern, "getMouseY", NULL,
            necro_ast_create_type_fn(arena,
                necro_ast_create_conid(arena, intern, "()", NECRO_CON_TYPE_VAR),
                necro_ast_create_conid(arena, intern, "Int", NECRO_CON_TYPE_VAR)),
            NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        necro_append_top(arena, top,
            necro_ast_create_apats_assignment(arena, intern, "getMouseY",
                necro_ast_create_apats(arena, necro_ast_create_var(arena, intern, "_dummy", NECRO_VAR_DECLARATION), NULL),
                necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "primUndefined", NECRO_VAR_VAR), NULL)));
    }

    // && / ||
    // NOTE: && and || turn into NecroMach binops 'and' and 'or', thus use primUndefined.
    {
        NecroAst* bool_conid = necro_ast_create_conid(arena, intern, "Bool", NECRO_CON_TYPE_VAR);
        necro_append_top(arena, top,
            necro_ast_create_fn_type_sig(arena, intern, "&&", NULL,
                necro_ast_create_type_fn(arena,
                    bool_conid,
                    necro_ast_create_type_fn(arena,
                        bool_conid,
                        bool_conid)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        NecroAst* and_apats = necro_ast_create_apats(arena, necro_ast_create_var(arena, intern, "x", NECRO_VAR_DECLARATION), necro_ast_create_apats(arena, necro_ast_create_var(arena, intern, "y", NECRO_VAR_DECLARATION), NULL));
        necro_append_top(arena, top, necro_ast_create_apats_assignment(arena, intern, "&&", and_apats, necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "primUndefined", NECRO_VAR_VAR), NULL)));
        necro_append_top(arena, top,
            necro_ast_create_fn_type_sig(arena, intern, "||", NULL,
                necro_ast_create_type_fn(arena,
                    bool_conid,
                    necro_ast_create_type_fn(arena,
                        bool_conid,
                        bool_conid)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        NecroAst* or_apats = necro_ast_create_apats(arena, necro_ast_create_var(arena, intern, "x", NECRO_VAR_DECLARATION), necro_ast_create_apats(arena, necro_ast_create_var(arena, intern, "y", NECRO_VAR_DECLARATION), NULL));
        necro_append_top(arena, top, necro_ast_create_apats_assignment(arena, intern, "||", or_apats, necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "primUndefined", NECRO_VAR_VAR), NULL)));
    }

    // //
    {
        necro_append_top(arena, top,
            necro_ast_create_fn_type_sig(arena, intern, "//", NULL,
                necro_ast_create_type_fn(arena,
                    necro_ast_create_conid(arena, intern, "Int", NECRO_CON_TYPE_VAR),
                    necro_ast_create_type_fn(arena,
                        necro_ast_create_conid(arena, intern, "Int", NECRO_CON_TYPE_VAR),
                        necro_ast_create_conid(arena, intern, "Rational", NECRO_CON_TYPE_VAR))),
                NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        NecroAst* ratio_apats = necro_ast_create_apats(arena, necro_ast_create_var(arena, intern, "x", NECRO_VAR_DECLARATION), necro_ast_create_apats(arena, necro_ast_create_var(arena, intern, "y", NECRO_VAR_DECLARATION), NULL));
        NecroAst* ratio_rhs   =
            necro_ast_create_fexpr(arena,
                necro_ast_create_fexpr(arena,
                    necro_ast_create_var(arena, intern, "rational", NECRO_VAR_VAR),
                    necro_ast_create_var(arena, intern, "x", NECRO_VAR_VAR)),
                necro_ast_create_var(arena, intern, "y", NECRO_VAR_VAR));
        necro_append_top(arena, top, necro_ast_create_apats_assignment(arena, intern, "//", ratio_apats, necro_ast_create_rhs(arena, ratio_rhs, NULL)));
    }

    //--------------------
    // Compile, part I
    necro_append_top_decl(top, file_top); // Append contents of base.necro
    necro_build_scopes(info, scoped_symtable, &base.ast);
    unwrap_or_print_error(void, necro_rename(info, scoped_symtable, intern, &base.ast), necro_base_lib_string, base_source_name);


    //--------------------
    // Cache useful symbols
    base.prim_undefined         = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "primUndefined"));;

    base.tuple2_con             = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "(,)"));
    base.tuple3_con             = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "(,,)"));
    base.tuple4_con             = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "(,,,)"));
    base.tuple5_con             = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "(,,,,)"));
    base.tuple6_con             = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "(,,,,,)"));
    base.tuple7_con             = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "(,,,,,,)"));
    base.tuple8_con             = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "(,,,,,,,)"));
    base.tuple9_con             = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "(,,,,,,,,)"));
    base.tuple10_con            = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "(,,,,,,,,,)"));

    base.tuple2_type            = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "(,)"));
    base.tuple3_type            = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "(,,)"));
    base.tuple4_type            = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "(,,,)"));
    base.tuple5_type            = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "(,,,,)"));
    base.tuple6_type            = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "(,,,,,)"));
    base.tuple7_type            = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "(,,,,,,)"));
    base.tuple8_type            = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "(,,,,,,,)"));
    base.tuple9_type            = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "(,,,,,,,,)"));
    base.tuple10_type           = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "(,,,,,,,,,)"));

    for (size_t i = 2; i <= NECRO_MAX_UNBOXED_TUPLE_TYPES; ++i)
    {
        char name[16] = "";
        snprintf(name, 16, "(#,#)%zu", i);
        base.unboxed_tuple_types[i]              = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, name));
        base.unboxed_tuple_cons[i]               = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, name));
        base.unboxed_tuple_types[i]->is_unboxed  = true;
        base.unboxed_tuple_types[i]->primop_type = NECRO_PRIMOP_UNBOXED_CON;
        base.unboxed_tuple_cons[i]->is_unboxed   = true;
        base.unboxed_tuple_cons[i]->primop_type  = NECRO_PRIMOP_UNBOXED_CON;
    }

    for (size_t i = 0; i < NECRO_MAX_ENV_TYPES; ++i)
    {
        char env_name[16] = "";
        snprintf(env_name, 16, "Env%zu", i);
        base.env_types[i] = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, env_name));
        base.env_cons[i]  = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, env_name));
    }

    // for (size_t i = 2; i < NECRO_MAX_BRANCH_TYPES; ++i)
    // {
    //     char branch_name[24] = "";
    //     snprintf(branch_name, 24, "BranchFn%zu", i);
    //     base.branch_types[i] = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, branch_name));
    //     base.branch_cons[i]  = necro_paged_arena_alloc(arena, i * sizeof(NecroAstSymbol*));
    //     for (size_t i2 = 0; i2 < i; ++i2)
    //     {
    //         snprintf(branch_name, 24, "BranchFn%zuAlt%zu", i, i2);
    //         base.branch_cons[i][i2] = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, branch_name));
    //     }
    // }

    //--------------------
    // Cache symbols
    base.world_type             = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "World"));
    base.unit_type              = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "()"));
    base.unit_con               = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "()"));
    base.seq_type               = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Seq"));
    base.seq_con                = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "Seq"));
    base.seq_value_type         = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "SeqValue"));
    // base.list_type              = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "[]"));
    base.int_type               = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Int"));
    base.int_con                = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "_Int"));
    base.uint_type              = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "UInt"));
    base.float_type             = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Float"));
    base.float_con              = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "_Float"));
    base.float_vec              = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "FloatVec"));
    base.char_type              = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Char"));
    base.char_con               = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "_Char"));
    base.bool_type              = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Bool"));
    base.true_con               = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "True"));
    base.false_con              = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "False"));

    base.semi_ring_class        = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Semiring"));
    base.ring_class             = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Ring"));
    base.division_ring_class    = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "DivisionRing"));
    base.euclidean_ring_class   = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "EuclideanRing"));
    base.field_class            = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Field"));
    base.num_type_class         = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Num"));
    base.integral_class         = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Integral"));
    base.bits_class             = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Bits"));
    base.floating_class         = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Floating"));
    // base.fractional_type_class  = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Fractional"));
    base.eq_type_class          = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Eq"));
    base.ord_type_class         = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Ord"));
    base.functor_type_class     = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Functor"));
    base.applicative_type_class = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Applicative"));
    base.monad_type_class       = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Monad"));
    base.default_type_class     = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Default"));
    base.audio_type_class       = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "AudioFormat"));
    base.mono_type              = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Mono"));
    base.prev_fn                = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "prev"));
    base.ptr_type               = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Ptr"));
    base.array_type             = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Array"));
    base.range_type             = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Range"));
    base.range_con              = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "Range"));
    base.index_type             = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Index"));
    base.index_con              = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "Index"));
    base.block_size_type        = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "BlockSize"));
    base.sample_rate_type       = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "SampleRate"));
    base.nat_mul_type           = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "NatMul"));
    base.nat_div_type           = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "NatDiv"));
    base.nat_max_type           = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "NatMax"));
    base.nat_next_power_of_2    = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "NatNextPowerOfTwo"));

    base.pipe_forward           = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "|>"));;
    base.pipe_back              = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "<|"));;
    base.compose_forward        = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, ".>"));;
    base.compose_back           = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "<."));;
    base.from_int               = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "fromInt"));;
    base.run_seq                = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "runSeq"));;
    base.seq_tick               = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "seqTick"));;
    base.tuple_tick             = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "tupleTick"));;
    base.interleave_tick        = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "interleaveTick"));;
    base.audio_sample_offset    = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "audioSampleOffset"));;

    //--------------------
    // Primitives
    base.prim_undefined->is_primitive = true;
    base.unit_type->is_primitive      = true;
    base.unit_con->is_primitive       = true;
    base.int_type->is_primitive       = true;
    base.uint_type->is_primitive      = true;
    base.float_type->is_primitive     = true;
    base.char_type->is_primitive      = true;
    base.bool_type->is_primitive      = true;
    base.true_con->is_primitive       = true;
    base.false_con->is_primitive      = true;
    base.array_type->is_primitive     = true;
    base.index_type->is_primitive     = true;
    base.ptr_type->is_primitive       = true;
    base.float_vec->is_primitive      = true;

    // Runtime Functions/Values
    necro_base_setup_primitive(scoped_symtable, intern, "panic",                    &base.panic,                       NECRO_PRIMOP_PRIM_FN);
    necro_base_setup_primitive(scoped_symtable, intern, "testAssertion",            &base.test_assertion,              NECRO_PRIMOP_PRIM_FN);
    necro_base_setup_primitive(scoped_symtable, intern, "getMouseX",                &base.mouse_x_fn,                  NECRO_PRIMOP_PRIM_FN);
    necro_base_setup_primitive(scoped_symtable, intern, "getMouseY",                &base.mouse_y_fn,                  NECRO_PRIMOP_PRIM_FN);
    necro_base_setup_primitive(scoped_symtable, intern, "sampleRate",               &base.sample_rate,                 NECRO_PRIMOP_PRIM_VAL);
    necro_base_setup_primitive(scoped_symtable, intern, "recipSampleRate",          &base.recip_sample_rate,           NECRO_PRIMOP_PRIM_VAL);
    necro_base_setup_primitive(scoped_symtable, intern, "printInt",                 &base.print_int,                   NECRO_PRIMOP_PRIM_FN);
    necro_base_setup_primitive(scoped_symtable, intern, "printUInt",                &base.print_uint,                  NECRO_PRIMOP_PRIM_FN);
    necro_base_setup_primitive(scoped_symtable, intern, "printFloat",               &base.print_float,                 NECRO_PRIMOP_PRIM_FN);
    necro_base_setup_primitive(scoped_symtable, intern, "printChar",                &base.print_char,                  NECRO_PRIMOP_PRIM_FN);
    necro_base_setup_primitive(scoped_symtable, intern, "outAudioBlock",            &base.out_audio_block,             NECRO_PRIMOP_PRIM_FN);
    necro_base_setup_primitive(scoped_symtable, intern, "recordAudioBlock",         &base.record_audio_block,          NECRO_PRIMOP_PRIM_FN);
    necro_base_setup_primitive(scoped_symtable, intern, "recordAudioBlockFinalize", &base.record_audio_block_finalize, NECRO_PRIMOP_PRIM_FN);
    necro_base_setup_primitive(scoped_symtable, intern, "unsafeAudioFileOpen",      &base.audio_file_open,             NECRO_PRIMOP_PRIM_FN);

    // File IO
    necro_base_setup_primitive(scoped_symtable, intern, "closeFile",        &base.close_file,          NECRO_PRIMOP_PRIM_FN);
    necro_base_setup_primitive(scoped_symtable, intern, "writeIntToFile",   &base.write_int_to_file,   NECRO_PRIMOP_PRIM_FN);
    necro_base_setup_primitive(scoped_symtable, intern, "writeUIntToFile",  &base.write_uint_to_file,  NECRO_PRIMOP_PRIM_FN);
    necro_base_setup_primitive(scoped_symtable, intern, "writeFloatToFile", &base.write_float_to_file, NECRO_PRIMOP_PRIM_FN);
    necro_base_setup_primitive(scoped_symtable, intern, "writeCharToFile",  &base.write_char_to_file,  NECRO_PRIMOP_PRIM_FN);
    necro_base_setup_primitive(scoped_symtable, intern, "unsafeOpenFile",   &base.open_file,           NECRO_PRIMOP_PRIM_FN);

    // Misc
    necro_base_setup_primitive(scoped_symtable, intern, "_project",         &base.proj_fn,           NECRO_PRIMOP_PROJ);
    necro_base_setup_primitive(scoped_symtable, intern, "unsafeArrayToPtr", NULL,                    NECRO_PRIMOP_UOP_BIT_CAST);

    // Intrinsics
    necro_base_setup_primitive(scoped_symtable, intern, "fma",              &base.fma,               NECRO_PRIMOP_INTR_FMA);

    // Array
    necro_base_setup_primitive(scoped_symtable, intern, "unsafeEmptyArray",  &base.unsafe_empty_array, NECRO_PRIMOP_ARRAY_EMPTY);
    necro_base_setup_primitive(scoped_symtable, intern, "readArray",         &base.read_array,         NECRO_PRIMOP_ARRAY_READ);
    necro_base_setup_primitive(scoped_symtable, intern, "readArrayU",        &base.read_arrayu,        NECRO_PRIMOP_ARRAY_READU);
    necro_base_setup_primitive(scoped_symtable, intern, "writeArray",        &base.write_array,        NECRO_PRIMOP_ARRAY_WRITE);
    necro_base_setup_primitive(scoped_symtable, intern, "freezeArray",       NULL,                     NECRO_PRIMOP_ARRAY_FREEZE);
    necro_base_setup_primitive(scoped_symtable, intern, "toFloatVecArray",   NULL,                     NECRO_PRIMOP_ARRAY_TO_FV);
    necro_base_setup_primitive(scoped_symtable, intern, "fromFloatVecArray", NULL,                     NECRO_PRIMOP_ARRAY_FROM_FV);

    // Ptr
    necro_base_setup_primitive(scoped_symtable, intern, "ptrMalloc",            NULL, NECRO_PRIMOP_PTR_ALLOC);
    necro_base_setup_primitive(scoped_symtable, intern, "ptrRealloc",           NULL, NECRO_PRIMOP_PTR_REALLOC);
    necro_base_setup_primitive(scoped_symtable, intern, "ptrFree",              NULL, NECRO_PRIMOP_PTR_FREE);
    necro_base_setup_primitive(scoped_symtable, intern, "unsafePtrPoke",        NULL, NECRO_PRIMOP_PTR_POKE);
    necro_base_setup_primitive(scoped_symtable, intern, "unsafePtrPeek",        NULL, NECRO_PRIMOP_PTR_PEEK);
    necro_base_setup_primitive(scoped_symtable, intern, "unsafePtrSwapElement", NULL, NECRO_PRIMOP_PTR_SWAP);
    necro_base_setup_primitive(scoped_symtable, intern, "unsafePtrCast",        NULL, NECRO_PRIMOP_PTR_CAST);

    necro_base_setup_primitive(scoped_symtable, intern, "mutRef", NULL, NECRO_PRIMOP_MREF);
    necro_base_setup_primitive(scoped_symtable, intern, "natVal", NULL, NECRO_PRIMOP_NAT_VAL);

    // PolyThunk
    necro_base_setup_primitive(scoped_symtable, intern, "polyThunkEval",   NULL, NECRO_PRIMOP_PTHUNK_EVAL);
    necro_base_setup_primitive(scoped_symtable, intern, "dynDeepCopy",     NULL, NECRO_PRIMOP_DYN_DCOPY);
    necro_base_setup_primitive(scoped_symtable, intern, "dynDeepCopyInto", NULL, NECRO_PRIMOP_DYN_DCOPY_INTO);

    // Int
    necro_base_setup_primitive(scoped_symtable, intern, "add<Int>",     NULL, NECRO_PRIMOP_BINOP_IADD);
    necro_base_setup_primitive(scoped_symtable, intern, "sub<Int>",     NULL, NECRO_PRIMOP_BINOP_ISUB);
    necro_base_setup_primitive(scoped_symtable, intern, "mul<Int>",     NULL, NECRO_PRIMOP_BINOP_IMUL);
    necro_base_setup_primitive(scoped_symtable, intern, "abs<Int>",     NULL, NECRO_PRIMOP_UOP_IABS);
    necro_base_setup_primitive(scoped_symtable, intern, "signum<Int>",  NULL, NECRO_PRIMOP_UOP_ISGN);
    necro_base_setup_primitive(scoped_symtable, intern, "fromInt<Int>", NULL, NECRO_PRIMOP_UOP_ITOI);
    necro_base_setup_primitive(scoped_symtable, intern, "div<Int>",     NULL, NECRO_PRIMOP_BINOP_IDIV);
    necro_base_setup_primitive(scoped_symtable, intern, "rem<Int>",     NULL, NECRO_PRIMOP_BINOP_IREM);
    necro_base_setup_primitive(scoped_symtable, intern, "eq<Int>",      NULL, NECRO_PRIMOP_CMP_EQ);
    necro_base_setup_primitive(scoped_symtable, intern, "neq<Int>",     NULL, NECRO_PRIMOP_CMP_NE);
    necro_base_setup_primitive(scoped_symtable, intern, "gt<Int>",      NULL, NECRO_PRIMOP_CMP_GT);
    necro_base_setup_primitive(scoped_symtable, intern, "lt<Int>",      NULL, NECRO_PRIMOP_CMP_LT);
    necro_base_setup_primitive(scoped_symtable, intern, "gte<Int>",     NULL, NECRO_PRIMOP_CMP_GE);
    necro_base_setup_primitive(scoped_symtable, intern, "lte<Int>",     NULL, NECRO_PRIMOP_CMP_LE);

    // UInt
    necro_base_setup_primitive(scoped_symtable, intern, "add<UInt>",            NULL, NECRO_PRIMOP_BINOP_UADD);
    necro_base_setup_primitive(scoped_symtable, intern, "sub<UInt>",            NULL, NECRO_PRIMOP_BINOP_USUB);
    necro_base_setup_primitive(scoped_symtable, intern, "mul<UInt>",            NULL, NECRO_PRIMOP_BINOP_UMUL);
    necro_base_setup_primitive(scoped_symtable, intern, "abs<UInt>",            NULL, NECRO_PRIMOP_UOP_UABS);
    necro_base_setup_primitive(scoped_symtable, intern, "signum<UInt>",         NULL, NECRO_PRIMOP_UOP_USGN);
    necro_base_setup_primitive(scoped_symtable, intern, "fromInt<UInt>",        NULL, NECRO_PRIMOP_UOP_ITOU);
    necro_base_setup_primitive(scoped_symtable, intern, "div<UInt>",            NULL, NECRO_PRIMOP_BINOP_UDIV);
    necro_base_setup_primitive(scoped_symtable, intern, "rem<UInt>",            NULL, NECRO_PRIMOP_BINOP_UREM);
    necro_base_setup_primitive(scoped_symtable, intern, "eq<UInt>",             NULL, NECRO_PRIMOP_CMP_EQ);
    necro_base_setup_primitive(scoped_symtable, intern, "neq<UInt>",            NULL, NECRO_PRIMOP_CMP_NE);
    necro_base_setup_primitive(scoped_symtable, intern, "gt<UInt>",             NULL, NECRO_PRIMOP_CMP_GT);
    necro_base_setup_primitive(scoped_symtable, intern, "lt<UInt>",             NULL, NECRO_PRIMOP_CMP_LT);
    necro_base_setup_primitive(scoped_symtable, intern, "gte<UInt>",            NULL, NECRO_PRIMOP_CMP_GE);
    necro_base_setup_primitive(scoped_symtable, intern, "lte<UInt>",            NULL, NECRO_PRIMOP_CMP_LE);
    necro_base_setup_primitive(scoped_symtable, intern, "bitAnd<UInt>",         NULL, NECRO_PRIMOP_BINOP_AND);
    necro_base_setup_primitive(scoped_symtable, intern, "bitNot<UInt>",         NULL, NECRO_PRIMOP_UOP_NOT);
    necro_base_setup_primitive(scoped_symtable, intern, "bitOr<UInt>",          NULL, NECRO_PRIMOP_BINOP_OR);
    necro_base_setup_primitive(scoped_symtable, intern, "bitXor<UInt>",         NULL, NECRO_PRIMOP_BINOP_XOR);
    necro_base_setup_primitive(scoped_symtable, intern, "bitShiftLeft<UInt>",   NULL, NECRO_PRIMOP_BINOP_SHL);
    necro_base_setup_primitive(scoped_symtable, intern, "bitShiftRight<UInt>",  NULL, NECRO_PRIMOP_BINOP_SHR);
    necro_base_setup_primitive(scoped_symtable, intern, "bitShiftRightA<UInt>", NULL, NECRO_PRIMOP_BINOP_SHRA);
    necro_base_setup_primitive(scoped_symtable, intern, "bitReverse<UInt>",     &base.bit_reverse_uint, NECRO_PRIMOP_INTR_BREV);
    necro_base_setup_primitive(scoped_symtable, intern, "uintToInt",            NULL, NECRO_PRIMOP_UOP_UTOI);

    // Float
    necro_base_setup_primitive(scoped_symtable, intern, "add<Float>",            NULL,                          NECRO_PRIMOP_BINOP_FADD);
    necro_base_setup_primitive(scoped_symtable, intern, "sub<Float>",            NULL,                          NECRO_PRIMOP_BINOP_FSUB);
    necro_base_setup_primitive(scoped_symtable, intern, "mul<Float>",            NULL,                          NECRO_PRIMOP_BINOP_FMUL);
    necro_base_setup_primitive(scoped_symtable, intern, "div<Float>",            NULL,                          NECRO_PRIMOP_BINOP_FDIV);
    necro_base_setup_primitive(scoped_symtable, intern, "rem<Float>",            NULL,                          NECRO_PRIMOP_BINOP_FREM);
    necro_base_setup_primitive(scoped_symtable, intern, "abs<Float>",            &base.abs_float,               NECRO_PRIMOP_INTR_FABS);
    necro_base_setup_primitive(scoped_symtable, intern, "fromInt<Float>",        NULL,                          NECRO_PRIMOP_UOP_ITOF);
    necro_base_setup_primitive(scoped_symtable, intern, "fromFloat<Float>",      NULL,                          NECRO_PRIMOP_UOP_FTOF);
    necro_base_setup_primitive(scoped_symtable, intern, "eq<Float>",             NULL,                          NECRO_PRIMOP_CMP_EQ);
    necro_base_setup_primitive(scoped_symtable, intern, "neq<Float>",            NULL,                          NECRO_PRIMOP_CMP_NE);
    necro_base_setup_primitive(scoped_symtable, intern, "gt<Float>",             NULL,                          NECRO_PRIMOP_CMP_GT);
    necro_base_setup_primitive(scoped_symtable, intern, "lt<Float>",             NULL,                          NECRO_PRIMOP_CMP_LT);
    necro_base_setup_primitive(scoped_symtable, intern, "gte<Float>",            NULL,                          NECRO_PRIMOP_CMP_GE);
    necro_base_setup_primitive(scoped_symtable, intern, "lte<Float>",            NULL,                          NECRO_PRIMOP_CMP_LE);
    necro_base_setup_primitive(scoped_symtable, intern, "bitAnd<Float>",         &base.bit_and_float,           NECRO_PRIMOP_BINOP_FAND);
    necro_base_setup_primitive(scoped_symtable, intern, "bitNot<Float>",         &base.bit_not_float,           NECRO_PRIMOP_UOP_FNOT);
    necro_base_setup_primitive(scoped_symtable, intern, "bitOr<Float>",          &base.bit_or_float,            NECRO_PRIMOP_BINOP_FOR);
    necro_base_setup_primitive(scoped_symtable, intern, "bitXor<Float>",         &base.bit_xor_float,           NECRO_PRIMOP_BINOP_FXOR);
    necro_base_setup_primitive(scoped_symtable, intern, "bitShiftLeft<Float>",   &base.bit_shift_left_float,    NECRO_PRIMOP_BINOP_FSHL);
    necro_base_setup_primitive(scoped_symtable, intern, "bitShiftRight<Float>",  &base.bit_shift_right_float,   NECRO_PRIMOP_BINOP_FSHR);
    necro_base_setup_primitive(scoped_symtable, intern, "bitShiftRightA<Float>", &base.bit_shift_right_a_float, NECRO_PRIMOP_BINOP_FSHRA);
    necro_base_setup_primitive(scoped_symtable, intern, "bitReverse<Float>",     &base.bit_reverse_float,       NECRO_PRIMOP_UOP_FBREV);
    necro_base_setup_primitive(scoped_symtable, intern, "toBits<Float>",         &base.to_bits_float,           NECRO_PRIMOP_UOP_FTOB);
    necro_base_setup_primitive(scoped_symtable, intern, "fromBits<Float>",       &base.from_bits_float,         NECRO_PRIMOP_UOP_FFRB);
    necro_base_setup_primitive(scoped_symtable, intern, "fastFloor",             &base.fast_floor,              NECRO_PRIMOP_UOP_FFLR);
    necro_base_setup_primitive(scoped_symtable, intern, "floorToInt",            &base.floor_to_int_float,      NECRO_PRIMOP_UOP_FFLR_TO_INT);
    necro_base_setup_primitive(scoped_symtable, intern, "ceilToInt",             &base.ceil_to_int_float,       NECRO_PRIMOP_UOP_FCEIL_TO_INT);
    necro_base_setup_primitive(scoped_symtable, intern, "truncateToInt",         &base.truncate_to_int_float,   NECRO_PRIMOP_UOP_FTRNC_TO_INT);
    necro_base_setup_primitive(scoped_symtable, intern, "roundToInt",            &base.round_to_int_float,      NECRO_PRIMOP_UOP_FRND_TO_INT);

    necro_base_setup_primitive(scoped_symtable, intern, "sine<Float>",           &base.sine_float,              NECRO_PRIMOP_INTR_SIN);
    necro_base_setup_primitive(scoped_symtable, intern, "cosine<Float>",         &base.cosine_float,            NECRO_PRIMOP_INTR_COS);
    necro_base_setup_primitive(scoped_symtable, intern, "exp<Float>",            &base.exp_float,               NECRO_PRIMOP_INTR_EXP);
    necro_base_setup_primitive(scoped_symtable, intern, "exp2<Float>",           &base.exp2_float,              NECRO_PRIMOP_INTR_EXP2);
    necro_base_setup_primitive(scoped_symtable, intern, "log<Float>",            &base.log_float,               NECRO_PRIMOP_INTR_LOG);
    necro_base_setup_primitive(scoped_symtable, intern, "log10<Float>",          &base.log10_float,             NECRO_PRIMOP_INTR_LOG10);
    necro_base_setup_primitive(scoped_symtable, intern, "log2<Float>",           &base.log2_float,              NECRO_PRIMOP_INTR_LOG2);
    necro_base_setup_primitive(scoped_symtable, intern, "pow<Float>",            &base.pow_float,               NECRO_PRIMOP_INTR_POW);
    necro_base_setup_primitive(scoped_symtable, intern, "sqrt<Float>",           &base.sqrt_float,              NECRO_PRIMOP_INTR_SQRT);
    necro_base_setup_primitive(scoped_symtable, intern, "floor<Float>",          &base.floor_float,             NECRO_PRIMOP_INTR_FFLR);
    necro_base_setup_primitive(scoped_symtable, intern, "ceil<Float>",           &base.ceil_float,              NECRO_PRIMOP_INTR_FCEIL);
    necro_base_setup_primitive(scoped_symtable, intern, "truncate<Float>",       &base.truncate_float,          NECRO_PRIMOP_INTR_FTRNC);
    necro_base_setup_primitive(scoped_symtable, intern, "round<Float>",          &base.round_float,             NECRO_PRIMOP_INTR_FRND);
    necro_base_setup_primitive(scoped_symtable, intern, "copysign<Float>",       &base.copy_sign_float,         NECRO_PRIMOP_INTR_FCPYSGN);
    /* necro_base_setup_primitive(scoped_symtable, intern, "fmin<Float>",           &base.min_float,             NECRO_PRIMOP_INTR_FMIN); */
    /* necro_base_setup_primitive(scoped_symtable, intern, "fmax<Float>",           &base.max_float,             NECRO_PRIMOP_INTR_FMAX); */
    necro_base_setup_primitive(scoped_symtable, intern, "floatToUInt",           NULL,                          NECRO_PRIMOP_UOP_FTRU);

    // FloatVec
    necro_base_setup_primitive(scoped_symtable, intern, "add<FloatVec>",       NULL, NECRO_PRIMOP_BINOP_FVADD);
    necro_base_setup_primitive(scoped_symtable, intern, "sub<FloatVec>",       NULL, NECRO_PRIMOP_BINOP_FVSUB);
    necro_base_setup_primitive(scoped_symtable, intern, "mul<FloatVec>",       NULL, NECRO_PRIMOP_BINOP_FVMUL);
    // necro_base_setup_primitive(scoped_symtable, intern, "abs<FloatVec>",     NULL, NECRO_PRIMOP_UOP_IABS);
    // necro_base_setup_primitive(scoped_symtable, intern, "signum<FloatVec>",  NULL, NECRO_PRIMOP_UOP_ISGN);
    necro_base_setup_primitive(scoped_symtable, intern, "fromInt<FloatVec>",   NULL, NECRO_PRIMOP_UOP_ITOFV);
    necro_base_setup_primitive(scoped_symtable, intern, "div<FloatVec>",       NULL, NECRO_PRIMOP_BINOP_FVDIV);
    necro_base_setup_primitive(scoped_symtable, intern, "rem<FloatVec>",       NULL, NECRO_PRIMOP_BINOP_FVREM);
    necro_base_setup_primitive(scoped_symtable, intern, "fromFloat<FloatVec>", NULL, NECRO_PRIMOP_UOP_FTOFV);
    // necro_base_setup_primitive(scoped_symtable, intern, "eq<FloatVec>",      NULL, NECRO_PRIMOP_CMP_EQ);
    // necro_base_setup_primitive(scoped_symtable, intern, "neq<FloatVec>",     NULL, NECRO_PRIMOP_CMP_NE);
    // necro_base_setup_primitive(scoped_symtable, intern, "gt<FloatVec>",      NULL, NECRO_PRIMOP_CMP_GT);
    // necro_base_setup_primitive(scoped_symtable, intern, "lt<FloatVec>",      NULL, NECRO_PRIMOP_CMP_LT);
    // necro_base_setup_primitive(scoped_symtable, intern, "gte<FloatVec>",     NULL, NECRO_PRIMOP_CMP_GE);
    // necro_base_setup_primitive(scoped_symtable, intern, "lte<FloatVec>",     NULL, NECRO_PRIMOP_CMP_LE);
    // necro_base_setup_primitive(scoped_symtable, intern, "floatVecInsert",    NULL, NECRO_PRIMOP_FV_INSERT);
    // necro_base_setup_primitive(scoped_symtable, intern, "floatVecExtract",   NULL, NECRO_PRIMOP_FV_EXTRACT);

    // ()
    necro_base_setup_primitive(scoped_symtable, intern, "eq<()>",  NULL, NECRO_PRIMOP_CMP_EQ);
    necro_base_setup_primitive(scoped_symtable, intern, "neq<()>", NULL, NECRO_PRIMOP_CMP_NE);
    necro_base_setup_primitive(scoped_symtable, intern, "gt<()>",  NULL, NECRO_PRIMOP_CMP_GT);
    necro_base_setup_primitive(scoped_symtable, intern, "lt<()>",  NULL, NECRO_PRIMOP_CMP_LT);
    necro_base_setup_primitive(scoped_symtable, intern, "gte<()>", NULL, NECRO_PRIMOP_CMP_GE);
    necro_base_setup_primitive(scoped_symtable, intern, "lte<()>", NULL, NECRO_PRIMOP_CMP_LE);

    // Bool
    necro_base_setup_primitive(scoped_symtable, intern, "eq<Bool>",  NULL, NECRO_PRIMOP_CMP_EQ);
    necro_base_setup_primitive(scoped_symtable, intern, "neq<Bool>", NULL, NECRO_PRIMOP_CMP_NE);
    necro_base_setup_primitive(scoped_symtable, intern, "gt<Bool>",  NULL, NECRO_PRIMOP_CMP_GT);
    necro_base_setup_primitive(scoped_symtable, intern, "lt<Bool>",  NULL, NECRO_PRIMOP_CMP_LT);
    necro_base_setup_primitive(scoped_symtable, intern, "gte<Bool>", NULL, NECRO_PRIMOP_CMP_GE);
    necro_base_setup_primitive(scoped_symtable, intern, "lte<Bool>", NULL, NECRO_PRIMOP_CMP_LE);
    necro_base_setup_primitive(scoped_symtable, intern, "&&",        NULL, NECRO_PRIMOP_BINOP_AND);
    necro_base_setup_primitive(scoped_symtable, intern, "||",        NULL, NECRO_PRIMOP_BINOP_OR);
    necro_base_setup_primitive(scoped_symtable, intern, "boolToInt", NULL, NECRO_PRIMOP_UOP_UTOI);

    // misc
    NecroAstSymbol* polyThunkEvalGo = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "polyThunkEvalGo"));;
    assert(polyThunkEvalGo != NULL);
    polyThunkEvalGo->never_inline = true;

    //--------------------
    // Compile, part II
    base.scoped_symtable = scoped_symtable;
    necro_dependency_analyze(info, intern, &base, &base.ast);
    necro_alias_analysis(info, &base.ast); // NOTE: Consider merging alias_analysis into RENAME_VAR phase?
    unwrap_or_print_error(void, necro_infer(info, intern, scoped_symtable, &base, &base.ast), necro_base_lib_string, base_source_name);
    unwrap_or_print_error(void, necro_monomorphize(info, intern, scoped_symtable, &base, &base.ast), necro_base_lib_string, base_source_name);

    // Finish
    return base;
}

NecroAstSymbol* necro_base_get_tuple_type(NecroBase* base, size_t num)
{
    switch (num)
    {
    case 2:  return base->tuple2_type;
    case 3:  return base->tuple3_type;
    case 4:  return base->tuple4_type;
    case 5:  return base->tuple5_type;
    case 6:  return base->tuple6_type;
    case 7:  return base->tuple7_type;
    case 8:  return base->tuple8_type;
    case 9:  return base->tuple9_type;
    case 10: return base->tuple10_type;
    default:
        assert(false && "Tuple arities above 10 are not supported");
        return NULL;
    }
}

NecroAstSymbol* necro_base_get_tuple_con(NecroBase* base, size_t num)
{
    switch (num)
    {
    case 2:  return base->tuple2_con;
    case 3:  return base->tuple3_con;
    case 4:  return base->tuple4_con;
    case 5:  return base->tuple5_con;
    case 6:  return base->tuple6_con;
    case 7:  return base->tuple7_con;
    case 8:  return base->tuple8_con;
    case 9:  return base->tuple9_con;
    case 10: return base->tuple10_con;
    default:
        assert(false && "Tuple arities above 10 are not supported");
        return NULL;
    }
}

// TODO: Instead of asserting this should be a user facing error!
// TODO: Dynamically create different arities!
NecroAstSymbol* necro_base_get_unboxed_tuple_type(NecroBase* base, size_t num)
{
    assert((num < NECRO_MAX_UNBOXED_TUPLE_TYPES) && "Unsupported Unboxed Tuple arity!");
    return base->unboxed_tuple_types[num];
}

NecroAstSymbol* necro_base_get_unboxed_tuple_con(NecroBase* base, size_t num)
{
    assert((num < NECRO_MAX_UNBOXED_TUPLE_TYPES) && "Unsupported Unboxed Tuple arity!");
    return base->unboxed_tuple_cons[num];
}

NecroAstSymbol* necro_base_get_env_type(NecroBase* base, size_t num)
{
    // TODO: Refactor to create env types dynamically, caching arities instead of blowing up at certain amounts?
    assert((num < NECRO_MAX_ENV_TYPES) && "Unsupported Env arity!");
    return base->env_types[num];
}

NecroAstSymbol* necro_base_get_env_con(NecroBase* base, size_t num)
{
    assert((num < NECRO_MAX_ENV_TYPES) && "Unsupported Env arity!");
    return base->env_cons[num];
}

NecroAstSymbol* necro_base_get_branch_type(NecroBase* base, size_t branch_size)
{
    assert((branch_size >= 2) && "Unsupported Branch Function arity!");
    assert((branch_size < NECRO_MAX_BRANCH_TYPES) && "Unsupported Branch Function arity!");
    return base->branch_types[branch_size];
}

NecroAstSymbol* necro_base_get_branch_con(NecroBase* base, size_t branch_size, size_t alternative)
{
    assert((branch_size >= 2) && "Unsupported Branch Function arity!");
    assert((branch_size < NECRO_MAX_BRANCH_TYPES) && "Unsupported Branch Function arity!");
    assert((alternative < branch_size) && "Alternative too large for Branch Function arity!");
    return base->branch_cons[branch_size][alternative];
}

NecroCoreAstSymbol* necro_base_get_proj_symbol(NecroPagedArena* arena, NecroBase* base)
{
    if (base->proj_fn->core_ast_symbol == NULL)
    {
        base->proj_fn->core_ast_symbol        = necro_core_ast_symbol_create_from_ast_symbol(arena, base->proj_fn);
        base->proj_fn->core_ast_symbol->arity = 2;
    }
    return base->proj_fn->core_ast_symbol;
}

bool necro_base_is_nat_op_type(const NecroBase* base, const NecroType* type)
{
    return
        type->con.con_symbol == base->block_size_type  ||
        type->con.con_symbol == base->nat_mul_type     ||
        type->con.con_symbol == base->nat_div_type     ||
        type->con.con_symbol == base->nat_max_type     ||
        type->con.con_symbol == base->sample_rate_type ||
        type->con.con_symbol == base->nat_next_power_of_2;
}

#define NECRO_BASE_TEST_VERBOSE 0

void necro_base_test()
{
    necro_announce_phase("NecroBase");

    // Set up
    NecroIntern         intern          = necro_intern_create();
    NecroAstArena       ast             = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
    NecroScopedSymTable scoped_symtable = necro_scoped_symtable_create();
    NecroBase           base            = necro_base_compile(&intern, &scoped_symtable);

    //--------------------
    // TODO list for Chad...
    //--------------------
    // Make tests that check that the base types are correct!!!!!

#if NECRO_BASE_TEST_VERBOSE
    necro_ast_arena_print(&base.ast);
    necro_scoped_symtable_print_top_scopes(&scoped_symtable);
#endif

    // Clean up
    necro_ast_arena_destroy(&ast);
    necro_base_destroy(&base);
    necro_scoped_symtable_destroy(&scoped_symtable);
    necro_intern_destroy(&intern);
}

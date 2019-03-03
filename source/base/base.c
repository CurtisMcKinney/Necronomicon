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

///////////////////////////////////////////////////////
// Create / Destroy
///////////////////////////////////////////////////////
NecroBase necro_base_create(NecroIntern* intern)
{
    return (NecroBase)
    {
        .ast                    = necro_ast_arena_create(necro_intern_string(intern, "Necro.Base")),

        .higher_kind            = NULL,
        .kind_kind              = NULL,
        .star_kind              = NULL,
        .nat_kind               = NULL,
        .sym_kind               = NULL,

        .tuple2_con             = NULL,
        .tuple3_con             = NULL,
        .tuple4_con             = NULL,
        .tuple5_con             = NULL,
        .tuple6_con             = NULL,
        .tuple7_con             = NULL,
        .tuple8_con             = NULL,
        .tuple9_con             = NULL,
        .tuple10_con            = NULL,

        .tuple2_type            = NULL,
        .tuple3_type            = NULL,
        .tuple4_type            = NULL,
        .tuple5_type            = NULL,
        .tuple6_type            = NULL,
        .tuple7_type            = NULL,
        .tuple8_type            = NULL,
        .tuple9_type            = NULL,
        .tuple10_type           = NULL,

        .poly_type              = NULL,
        .world_type             = NULL,
        .world_value            = NULL,
        .unit_type              = NULL,
        .unit_con               = NULL,
        // .list_type              = NULL,
        .int_type               = NULL,
        .float_type             = NULL,
        .audio_type             = NULL,
        .rational_type          = NULL,
        .char_type              = NULL,
        .bool_type              = NULL,
        .num_type_class         = NULL,
        .fractional_type_class  = NULL,
        .eq_type_class          = NULL,
        .ord_type_class         = NULL,
        .functor_type_class     = NULL,
        .applicative_type_class = NULL,
        .monad_type_class       = NULL,
        .default_type_class     = NULL,
        .event_type             = NULL,
        .pattern_type           = NULL,
        .closure_type           = NULL,
        // .apply_fn               = NULL,
        // .dyn_state_type         = NULL,
        .ptr_type               = NULL,
        .array_type             = NULL,
        .maybe_type             = NULL,

        .mouse_x_fn             = NULL,
        .mouse_y_fn             = NULL,
        .unsafe_malloc          = NULL,
        .unsafe_peek            = NULL,
        .unsafe_poke            = NULL,
    };
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

void necro_base_create_simple_data_decl(NecroPagedArena* arena, NecroAst* top, NecroIntern* intern, const char* data_type_name)
{
    NecroAst* s_type   = necro_ast_create_simple_type(arena, intern, data_type_name, NULL);
    NecroAst* n_con    = necro_ast_create_data_con(arena, intern, data_type_name, NULL);
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

NecroAst* necro_base_create_num_bin_op_sig(NecroPagedArena* arena, NecroIntern* intern, const char* sig_name)
{
    return
        necro_ast_create_fn_type_sig(arena, intern, sig_name, necro_ast_create_context(arena, intern, "Num", "a", NULL),
            necro_ast_create_type_fn(arena,
                necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                necro_ast_create_type_fn(arena,
                    necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                    necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR))), NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
}

NecroAst* necro_base_create_fractional_bin_op_sig(NecroPagedArena* arena, NecroIntern* intern, const char* sig_name)
{
    return
        necro_ast_create_fn_type_sig(arena, intern, sig_name, necro_ast_create_context(arena, intern, "Fractional", "a", NULL),
            necro_ast_create_type_fn(arena,
                necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                necro_ast_create_type_fn(arena,
                    necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                    necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR))), NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
}

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
            necro_ast_create_var(arena, intern, "_primUndefined", NECRO_VAR_VAR),
            NULL));
}

NecroAst* necro_ast_create_prim_unary_op_method_ast(NecroPagedArena* arena, NecroIntern* intern, const char* name)
{
    return necro_ast_create_apats_assignment(arena, intern, name,
        necro_ast_create_apats(arena,
            necro_ast_create_var(arena, intern, "x", NECRO_VAR_DECLARATION),
            NULL),
        necro_ast_create_rhs(arena,
            necro_ast_create_var(arena, intern, "_primUndefined", NECRO_VAR_VAR),
            NULL));
}

void necro_base_create_prim_num_instances(NecroPagedArena* arena, NecroAst* top, NecroIntern* intern, const char* data_type_name, bool is_poly, bool is_num, bool is_fractional)
{
    //--------------
    // Num
    if (is_num)
    {
        NecroAst* num_method_list  =
            necro_ast_create_decl(arena, necro_ast_create_prim_bin_op_method_ast(arena, intern, "add"),
                necro_ast_create_decl(arena, necro_ast_create_prim_bin_op_method_ast(arena, intern, "sub"),
                    necro_ast_create_decl(arena, necro_ast_create_prim_bin_op_method_ast(arena, intern, "mul"),
                        necro_ast_create_decl(arena, necro_ast_create_prim_unary_op_method_ast(arena, intern, "abs"),
                            necro_ast_create_decl(arena, necro_ast_create_prim_unary_op_method_ast(arena, intern, "signum"),
                                necro_ast_create_decl(arena, necro_ast_create_prim_unary_op_method_ast(arena, intern, "fromInt"), NULL))))));
        necro_append_top(arena, top, necro_ast_create_instance(arena, intern, "Num", necro_base_make_num_con(arena, intern, data_type_name, is_poly), necro_base_make_num_context(arena, intern, "Num", is_poly), num_method_list));
    }

    //--------------
    // Fractional
    if (is_fractional)
    {
        NecroAst* fractional_method_list =
            necro_ast_create_decl(arena, necro_ast_create_prim_bin_op_method_ast(arena, intern, "div"),
                necro_ast_create_decl(arena, necro_ast_create_prim_unary_op_method_ast(arena, intern, "recip"),
                    necro_ast_create_decl(arena, necro_ast_create_prim_unary_op_method_ast(arena, intern, "fromRational"), NULL)));
        necro_append_top(arena, top, necro_ast_create_instance(arena, intern, "Fractional", necro_base_make_num_con(arena, intern, data_type_name, is_poly), necro_base_make_num_context(arena, intern, "Fractional", is_poly), fractional_method_list));
    }

    //--------------
    // Eq
    NecroAst* eq_method_list =
        necro_ast_create_decl(arena, necro_ast_create_prim_bin_op_method_ast(arena, intern, "eq"),
            necro_ast_create_decl(arena, necro_ast_create_prim_bin_op_method_ast(arena, intern, "neq"), NULL));

    necro_append_top(arena, top, necro_ast_create_instance(arena, intern, "Eq", necro_base_make_num_con(arena, intern, data_type_name, is_poly), necro_base_make_num_context(arena, intern, "Eq", is_poly), eq_method_list));

    //--------------
    // Ord
    NecroAst* ord_method_list =
        necro_ast_create_decl(arena, necro_ast_create_prim_bin_op_method_ast(arena, intern, "lt"),
            necro_ast_create_decl(arena, necro_ast_create_prim_bin_op_method_ast(arena, intern, "gt"),
                necro_ast_create_decl(arena, necro_ast_create_prim_bin_op_method_ast(arena, intern, "lte"),
                    necro_ast_create_decl(arena, necro_ast_create_prim_bin_op_method_ast(arena, intern, "gte"), NULL))));
    necro_append_top(arena, top, necro_ast_create_instance(arena, intern, "Ord", necro_base_make_num_con(arena, intern, data_type_name, is_poly), necro_base_make_num_context(arena, intern, "Ord", is_poly), ord_method_list));


    //--------------
    // Default
    if (is_num)
    {
        NecroAst* default_method_ast  =
            necro_ast_create_simple_assignment(arena, intern, "default",
                necro_ast_create_rhs(arena,
                    necro_ast_create_fexpr(arena,
                        necro_ast_create_var(arena, intern, "fromInt", NECRO_VAR_VAR),
                        necro_ast_create_constant(arena, (NecroParseAstConstant) { .type = NECRO_AST_CONSTANT_INTEGER, .int_literal = 0 })),
                    NULL
                ));
        NecroAst* default_method_list = necro_ast_create_decl(arena, default_method_ast, NULL);
        necro_append_top(arena, top, necro_ast_create_instance(arena, intern, "Default", necro_base_make_num_con(arena, intern, data_type_name, is_poly), necro_base_make_num_context(arena, intern, "Num", is_poly), default_method_list));
    }
}

NecroBase necro_base_compile(NecroIntern* intern, NecroScopedSymTable* scoped_symtable)
{
    NecroCompileInfo info  = (NecroCompileInfo) { .verbosity = 0, .timer = NULL, .opt_level = NECRO_OPT_OFF, .compilation_phase = NECRO_PHASE_JIT };
    NecroBase        base  = necro_base_create(intern);
    NecroPagedArena* arena = &base.ast.arena;
    NecroAst*        top   = NULL;

    necro_kind_init_kinds(&base, scoped_symtable, intern);

    // _Poly
    NecroAst* poly_s_type            = necro_ast_create_simple_type(arena, intern, "_Poly", NULL);
    NecroAst* poly_null              = necro_ast_create_data_con(arena, intern, "_NullPoly", NULL);
    NecroAst* poly_constructor_list  = necro_ast_create_list(arena, poly_null, NULL);
    NecroAst* poly_data_decl         = necro_ast_create_data_declaration(arena, intern, poly_s_type, poly_constructor_list);
    top                              = necro_ast_create_top_decl(arena, poly_data_decl, NULL);
    base.ast.root                    = top;

    // _primUndefined
    // necro_append_top(arena, top, necro_ast_create_fn_type_sig(arena, intern, "_primUndefined", NULL, necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR), NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
    necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "_primUndefined", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "_primUndefined", NECRO_VAR_VAR), NULL)));

    // ()
    NecroAst* unit_s_type            = necro_ast_create_simple_type(arena, intern, "()", NULL);
    NecroAst* unit_constructor       = necro_ast_create_data_con(arena, intern, "()", NULL);
    NecroAst* unit_constructor_list  = necro_ast_create_list(arena, unit_constructor, NULL);
    necro_append_top(arena, top, necro_ast_create_data_declaration(arena, intern, unit_s_type, unit_constructor_list));

    {
        // TODO: Make proper instance methods for (), not relying on _primUndefined
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

    // Maybe
    NecroAst* maybe_s_type           = necro_ast_create_simple_type(arena, intern, "Maybe", necro_ast_create_var_list(arena, intern, 1, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroAst* just_constructor       = necro_ast_create_data_con(arena, intern, "Just", necro_ast_create_list(arena, necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR), NULL));
    NecroAst* nothing_constructor    = necro_ast_create_data_con(arena, intern, "Nothing", NULL);
    NecroAst* maybe_constructor_list = necro_ast_create_list(arena, just_constructor, necro_ast_create_list(arena, nothing_constructor, NULL));
    necro_append_top(arena, top, necro_ast_create_data_declaration(arena, intern, maybe_s_type, maybe_constructor_list));

    // Bool
    NecroAst* bool_s_type            = necro_ast_create_simple_type(arena, intern, "Bool", NULL);
    NecroAst* false_constructor      = necro_ast_create_data_con(arena, intern, "False", NULL);
    NecroAst* true_constructor       = necro_ast_create_data_con(arena, intern, "True", NULL);
    NecroAst* bool_constructor_list  = necro_ast_create_list(arena, false_constructor, necro_ast_create_list(arena, true_constructor, NULL));
    necro_append_top(arena, top, necro_ast_create_data_declaration(arena, intern, bool_s_type, bool_constructor_list));

    // Simple Data Decls
    necro_base_create_simple_data_decl(arena, top, intern, "World");
    necro_base_create_simple_data_decl(arena, top, intern, "Event");
    necro_base_create_simple_data_decl(arena, top, intern, "Int");
    necro_base_create_simple_data_decl(arena, top, intern, "Float");
    necro_base_create_simple_data_decl(arena, top, intern, "Audio");
    necro_base_create_simple_data_decl(arena, top, intern, "Rational");
    necro_base_create_simple_data_decl(arena, top, intern, "Char");

    // Simple Poly Data Decls
    necro_base_create_simple_poly_data_decl(arena, top, intern, "Pattern");
    necro_base_create_simple_poly_data_decl(arena, top, intern, "Ptr");

    // Array
    NecroAst* array_n_type   = necro_ast_create_type_signature(arena, NECRO_SIG_TYPE_VAR, necro_ast_create_var(arena, intern, "n", NECRO_VAR_TYPE_VAR_DECLARATION), NULL, necro_ast_create_conid(arena, intern, "Nat", NECRO_CON_TYPE_VAR));
    NecroAst* array_s_type   = necro_ast_create_simple_type(arena, intern, "Array", necro_ast_create_list(arena, array_n_type, necro_ast_create_var_list(arena, intern, 1, NECRO_VAR_TYPE_VAR_DECLARATION)));
    NecroAst* array_args     =
            necro_ast_create_list(arena,
                necro_ast_create_type_app(arena,
                    necro_ast_create_conid(arena, intern, "Ptr", NECRO_CON_TYPE_VAR),
                    necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR)),
                NULL);
    NecroAst* array_con      = necro_ast_create_data_con(arena, intern, "Array", array_args);
    NecroAst* array_con_list = necro_ast_create_list(arena, array_con, NULL);
    necro_append_top(arena, top, necro_ast_create_data_declaration(arena, intern, array_s_type, array_con_list));

    // NOTE: Removing with new function restriction in place for futhark style defunctionalization
    // // _Closure
    // NecroAst* closure_s_type   = necro_ast_create_simple_type(arena, intern, "_Closure", necro_ast_create_var_list(arena, intern, 1, NECRO_VAR_TYPE_VAR_DECLARATION));
    // NecroAst* closure_args     =
    //     necro_ast_create_list(arena, necro_ast_create_conid(arena, intern, "Int", NECRO_CON_TYPE_VAR),
    //         necro_ast_create_list(arena, necro_ast_create_conid(arena, intern, "Int", NECRO_CON_TYPE_VAR),
    //             necro_ast_create_list(arena, necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR), NULL)));
    // NecroAst* closure_con      = necro_ast_create_data_con(arena, intern, "_Closure", closure_args);
    // NecroAst* closure_con_list = necro_ast_create_list(arena, closure_con, NULL);
    // necro_append_top(arena, top, necro_ast_create_data_declaration(arena, intern, closure_s_type, closure_con_list));

    // // _apply
    // NecroAst* apply_type_ast  = necro_ast_create_type_fn(arena, necro_ast_create_type_app(arena, necro_ast_create_conid(arena, intern, "_Closure", NECRO_CON_TYPE_VAR), necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR)), necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR));
    // necro_append_top(arena, top, necro_ast_create_fn_type_sig(arena, intern, "_apply", NULL, apply_type_ast, NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
    // necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "_apply", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "_primUndefined", NECRO_VAR_VAR), NULL)));

    // // _DynState
    // NecroAst* dyn_state_s_type           = necro_ast_create_simple_type(arena, intern, "_DynState", necro_ast_create_var_list(arena, intern, 1, NECRO_VAR_TYPE_VAR_DECLARATION));
    // NecroAst* dyn_state_constructor      = necro_ast_create_data_con(arena, intern, "_DynState",
    //     necro_ast_create_list(arena, necro_ast_create_conid(arena, intern, "_Poly", NECRO_CON_TYPE_VAR),
    //         necro_ast_create_list(arena, necro_ast_create_conid(arena, intern, "Int", NECRO_CON_TYPE_VAR), NULL)));
    // NecroAst* dyn_state_constructor_list = necro_ast_create_list(arena, dyn_state_constructor, NULL);
    // necro_append_top(arena, top, necro_ast_create_data_declaration(arena, intern, dyn_state_s_type, dyn_state_constructor_list));

    // Eq
    NecroAst* eq_method_sig  = necro_base_create_class_comp_sig(arena, intern, "eq");
    NecroAst* neq_method_sig = necro_base_create_class_comp_sig(arena, intern, "neq");
    NecroAst* eq_method_list = necro_ast_create_decl(arena, eq_method_sig, necro_ast_create_decl(arena, neq_method_sig, NULL));
    NecroAst* eq_class_ast   = necro_ast_create_type_class(arena, intern, "Eq", "a", NULL, eq_method_list);
    necro_append_top(arena, top, eq_class_ast);

    // Ord
    NecroAst* gt_method_sig   = necro_base_create_class_comp_sig(arena, intern, "gt");
    NecroAst* lt_method_sig   = necro_base_create_class_comp_sig(arena, intern, "lt");
    NecroAst* gte_method_sig  = necro_base_create_class_comp_sig(arena, intern, "gte");
    NecroAst* lte_method_sig  = necro_base_create_class_comp_sig(arena, intern, "lte");
    NecroAst* ord_method_list =
        necro_ast_create_decl(arena, gt_method_sig,
            necro_ast_create_decl(arena, lt_method_sig,
                necro_ast_create_decl(arena, gte_method_sig,
                    necro_ast_create_decl(arena, lte_method_sig, NULL))));
    NecroAst* ord_class_ast   = necro_ast_create_type_class(arena, intern, "Ord", "a", necro_ast_create_context(arena, intern, "Eq", "a", NULL), ord_method_list);
    necro_append_top(arena, top, ord_class_ast);

    // Num
    NecroAst* add_method_sig    = necro_base_create_class_bin_op_sig(arena, intern, "add");
    NecroAst* sub_method_sig    = necro_base_create_class_bin_op_sig(arena, intern, "sub");
    NecroAst* mul_method_sig    = necro_base_create_class_bin_op_sig(arena, intern, "mul");
    NecroAst* abs_method_sig    = necro_base_create_class_unary_op_sig(arena, intern, "abs");
    NecroAst* signum_method_sig = necro_base_create_class_unary_op_sig(arena, intern, "signum");
    NecroAst* from_int_sig      =
        necro_ast_create_fn_type_sig(arena, intern, "fromInt", NULL, necro_ast_create_type_fn(arena,
            necro_ast_create_conid(arena, intern, "Int", NECRO_CON_TYPE_VAR),
            necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR)), NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
    NecroAst* num_method_list =
        necro_ast_create_decl(arena, add_method_sig,
            necro_ast_create_decl(arena, sub_method_sig,
                necro_ast_create_decl(arena, mul_method_sig,
                    necro_ast_create_decl(arena, abs_method_sig,
                        necro_ast_create_decl(arena, signum_method_sig,
                            necro_ast_create_decl(arena, from_int_sig, NULL))))));
    NecroAst* num_class_ast = necro_ast_create_type_class(arena, intern, "Num", "a", NULL, num_method_list);
    necro_append_top(arena, top, num_class_ast);

    // Frac
    NecroAst* div_method_sig    = necro_base_create_class_bin_op_sig(arena, intern, "div");
    NecroAst* recip_method_sig  = necro_base_create_class_unary_op_sig(arena, intern, "recip");
    NecroAst* from_rational_sig =
        necro_ast_create_fn_type_sig(arena, intern, "fromRational", NULL, necro_ast_create_type_fn(arena,
            necro_ast_create_conid(arena, intern, "Float", NECRO_CON_TYPE_VAR),
            necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR)), NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
    NecroAst* fractional_method_list =
        necro_ast_create_decl(arena, div_method_sig,
            necro_ast_create_decl(arena, recip_method_sig,
                necro_ast_create_decl(arena, from_rational_sig, NULL)));
    NecroAst* fractional_class_ast = necro_ast_create_type_class(arena, intern, "Fractional", "a", necro_ast_create_context(arena, intern, "Num", "a", NULL), fractional_method_list);
    necro_append_top(arena, top, fractional_class_ast);

    // TODO: Put Static constraints on this
    NecroAst* default_type_sig    = necro_ast_create_fn_type_sig(arena, intern, "default", NULL, necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR), NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
    NecroAst* default_method_list = necro_ast_create_decl(arena, default_type_sig, NULL);
    NecroAst* default_class_ast   = necro_ast_create_type_class(arena, intern, "Default", "a", NULL, default_method_list);
    necro_append_top(arena, top, default_class_ast);

    // Num instances
    necro_base_create_prim_num_instances(arena, top, intern, "Int", false, true, false);
    necro_base_create_prim_num_instances(arena, top, intern, "Float", false, true, true);
    necro_base_create_prim_num_instances(arena, top, intern, "Audio", false, true, true);
    necro_base_create_prim_num_instances(arena, top, intern, "Rational", false, true, true);
    necro_base_create_prim_num_instances(arena, top, intern, "Pattern", true, true, true);
    necro_base_create_prim_num_instances(arena, top, intern, "Bool", false, false, false);

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

    // Functor
    {
        NecroAst* a_var          = necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* b_var          = necro_ast_create_var(arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* f_var          = necro_ast_create_var(arena, intern, "f", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* map_method_sig =
            necro_ast_create_fn_type_sig(arena, intern, "map", NULL,
                necro_ast_create_type_fn(arena,
                    necro_ast_create_type_fn(arena, a_var, b_var),
                    necro_ast_create_type_fn(arena,
                        necro_ast_create_type_app(arena, f_var, a_var),
                        necro_ast_create_type_app(arena, f_var, b_var))),
                    NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
        NecroAst* functor_method_list = necro_ast_create_decl(arena, map_method_sig, NULL);
        necro_append_top(arena, top, necro_ast_create_type_class(arena, intern, "Functor", "f", NULL, functor_method_list));
    }

    // Applicative
    {
        NecroAst* a_var           = necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* b_var           = necro_ast_create_var(arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* f_var           = necro_ast_create_var(arena, intern, "f", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* pure_method_sig =
            necro_ast_create_fn_type_sig(arena, intern, "pure", NULL, necro_ast_create_type_fn(arena, a_var, necro_ast_create_type_app(arena, f_var, a_var)), NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
        NecroAst* ap_method_sig  =
            necro_ast_create_fn_type_sig(arena, intern, "ap", NULL,
                necro_ast_create_type_fn(arena,
                    necro_ast_create_type_app(arena, f_var, necro_ast_create_type_fn(arena, a_var, b_var)),
                    necro_ast_create_type_fn(arena,
                        necro_ast_create_type_app(arena, f_var, a_var),
                        necro_ast_create_type_app(arena, f_var, b_var))),
                    NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
        NecroAst* applicative_method_list = necro_ast_create_decl(arena, pure_method_sig, necro_ast_create_decl(arena, ap_method_sig, NULL));
        necro_append_top(arena, top, necro_ast_create_type_class(arena, intern, "Applicative", "f", necro_ast_create_context(arena, intern, "Functor", "f", NULL), applicative_method_list));
    }

    // Monad
    {
        NecroAst* a_var           = necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* b_var           = necro_ast_create_var(arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* m_var           = necro_ast_create_var(arena, intern, "m", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* bind_method_sig  =
            necro_ast_create_fn_type_sig(arena, intern, "bind", NULL,
                necro_ast_create_type_fn(arena,
                    necro_ast_create_type_app(arena, m_var, a_var),
                    necro_ast_create_type_fn(arena,
                        necro_ast_create_type_fn(arena, a_var, necro_ast_create_type_app(arena, m_var, b_var)),
                        necro_ast_create_type_app(arena, m_var, b_var))),
                    NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
        NecroAst* monad_method_list = necro_ast_create_decl(arena, bind_method_sig, NULL);
        necro_append_top(arena, top, necro_ast_create_type_class(arena, intern, "Monad", "m", necro_ast_create_context(arena, intern, "Applicative", "m", NULL), monad_method_list));
    }

    // Semigroup
    {
        NecroAst* a_var             = necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* append_method_sig =
            necro_ast_create_fn_type_sig(arena, intern, "append", NULL,
                necro_ast_create_type_fn(arena, a_var, necro_ast_create_type_fn(arena, a_var, a_var)),
                    NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
        NecroAst* semigroup_method_list = necro_ast_create_decl(arena, append_method_sig, NULL);
        necro_append_top(arena, top, necro_ast_create_type_class(arena, intern, "Semigroup", "a", NULL, semigroup_method_list));
    }

    // Monoid
    {
        NecroAst* a_var             = necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* mempty_method_sig =
            necro_ast_create_fn_type_sig(arena, intern, "mempty", NULL,
                a_var,
                    NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
        NecroAst* monoid_method_list = necro_ast_create_decl(arena, mempty_method_sig, NULL);
        necro_append_top(arena, top, necro_ast_create_type_class(arena, intern, "Monoid", "a", necro_ast_create_context(arena, intern, "Semigroup", "a", NULL), monoid_method_list));
    }

    //--------------------
    // BinOps
    //--------------------

    // +
    necro_append_top(arena, top, necro_base_create_num_bin_op_sig(arena, intern, "+"));
    necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "+", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "add", NECRO_VAR_VAR), NULL)));

    // -
    necro_append_top(arena, top, necro_base_create_num_bin_op_sig(arena, intern, "-"));
    necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "-", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "sub", NECRO_VAR_VAR), NULL)));

    // *
    necro_append_top(arena, top, necro_base_create_num_bin_op_sig(arena, intern, "*"));
    necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "*", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "mul", NECRO_VAR_VAR), NULL)));

    // /
    necro_append_top(arena, top, necro_base_create_fractional_bin_op_sig(arena, intern, "/"));
    necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "/", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "div", NECRO_VAR_VAR), NULL)));

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

    // >>=
    {
        NecroAst* a_var       = necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* b_var       = necro_ast_create_var(arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* m_var       = necro_ast_create_var(arena, intern, "m", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* bind_op_sig =
            necro_ast_create_fn_type_sig(arena, intern, ">>=", necro_ast_create_context(arena, intern, "Monad", "m", NULL),
                necro_ast_create_type_fn(arena,
                    necro_ast_create_type_app(arena, m_var, a_var),
                    necro_ast_create_type_fn(arena,
                        necro_ast_create_type_fn(arena, a_var, necro_ast_create_type_app(arena, m_var, b_var)),
                        necro_ast_create_type_app(arena, m_var, b_var))),
                NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        necro_append_top(arena, top, bind_op_sig);
        necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, ">>=", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "bind", NECRO_VAR_VAR), NULL)));
    }

    // TODO: Make >> and << composition operators instead
    // >>
    {
        NecroAst* a_var       = necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* b_var       = necro_ast_create_var(arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* m_var       = necro_ast_create_var(arena, intern, "m", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* op_sig =
            necro_ast_create_fn_type_sig(arena, intern, ">>", necro_ast_create_context(arena, intern, "Monad", "m", NULL),
                necro_ast_create_type_fn(arena,
                    necro_ast_create_type_app(arena, m_var, a_var),
                    necro_ast_create_type_fn(arena,
                        necro_ast_create_type_app(arena, m_var, b_var),
                        necro_ast_create_type_app(arena, m_var, b_var))),
                NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        NecroAst* mm_var      = necro_ast_create_var(arena, intern, "m", NECRO_VAR_DECLARATION);
        NecroAst* mk_var      = necro_ast_create_var(arena, intern, "k", NECRO_VAR_DECLARATION);
        NecroAst* op_args     = necro_ast_create_apats(arena, mm_var, necro_ast_create_apats(arena, mk_var, NULL));
        NecroAst* bind        = necro_ast_create_fexpr(arena,
            necro_ast_create_fexpr(arena, necro_ast_create_var(arena, intern, "bind", NECRO_VAR_VAR), necro_ast_create_var(arena, intern, "m", NECRO_VAR_VAR)),
            necro_ast_create_lambda(arena, necro_ast_create_apats(arena, necro_ast_create_wildcard(arena), NULL), necro_ast_create_var(arena, intern, "k", NECRO_VAR_VAR)));
        NecroAst* op_rhs      = necro_ast_create_rhs(arena, bind, NULL);
        NecroAst* op_def_ast  = necro_ast_create_apats_assignment(arena, intern, ">>", op_args, op_rhs);
        necro_append_top(arena, top, op_sig);
        necro_append_top(arena, top, op_def_ast);
    }

    // <|
    {
        NecroAst* a_var       = necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* b_var       = necro_ast_create_var(arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        a_var->variable.order = NECRO_TYPE_HIGHER_ORDER;
        b_var->variable.order = NECRO_TYPE_HIGHER_ORDER;
        NecroAst* op_sig      =
            necro_ast_create_fn_type_sig(arena, intern, "<|", NULL,
                necro_ast_create_type_fn(arena, necro_ast_create_type_fn(arena, a_var, b_var),
                    necro_ast_create_type_fn(arena, a_var, b_var)),
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
                necro_ast_create_type_fn(arena, a_var,
                    necro_ast_create_type_fn(arena, necro_ast_create_type_fn(arena, a_var, b_var), b_var)),
                NECRO_VAR_SIG, NECRO_SIG_DECLARATION);
        NecroAst* x_var       = necro_ast_create_var(arena, intern, "x", NECRO_VAR_DECLARATION);
        NecroAst* f_var       = necro_ast_create_var(arena, intern, "f", NECRO_VAR_DECLARATION);
        NecroAst* op_args     = necro_ast_create_apats(arena, x_var, necro_ast_create_apats(arena, f_var, NULL));
        NecroAst* op_rhs_ast  = necro_ast_create_rhs(arena, necro_ast_create_fexpr(arena, necro_ast_create_var(arena, intern, "f", NECRO_VAR_VAR), necro_ast_create_var(arena, intern, "x", NECRO_VAR_VAR)), NULL);
        NecroAst* op_def_ast  = necro_ast_create_apats_assignment(arena, intern, "|>", op_args, op_rhs_ast);
        necro_append_top(arena, top, op_sig);
        necro_append_top(arena, top, op_def_ast);
    }

    // .
    {
        NecroAst* a_var       = necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* b_var       = necro_ast_create_var(arena, intern, "b", NECRO_VAR_TYPE_FREE_VAR);
        NecroAst* c_var       = necro_ast_create_var(arena, intern, "c", NECRO_VAR_TYPE_FREE_VAR);
        a_var->variable.order = NECRO_TYPE_HIGHER_ORDER;
        b_var->variable.order = NECRO_TYPE_HIGHER_ORDER;
        c_var->variable.order = NECRO_TYPE_HIGHER_ORDER;
        NecroAst* op_sig      =
            necro_ast_create_fn_type_sig(arena, intern, ".", NULL,
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
        NecroAst* op_def_ast  = necro_ast_create_apats_assignment(arena, intern, ".", op_args, op_rhs_ast);
        necro_append_top(arena, top, op_sig);
        necro_append_top(arena, top, op_def_ast);
    }

    // world value
    {
        necro_append_top(arena, top, necro_ast_create_fn_type_sig(arena, intern, "world", NULL, necro_ast_create_conid(arena, intern, "World", NECRO_CON_TYPE_VAR), NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "world", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "_primUndefined", NECRO_VAR_VAR), NULL)));
    }

    // mouseX
    {
        necro_append_top(arena, top, necro_ast_create_fn_type_sig(arena, intern, "getMouseX", NULL,
            necro_ast_create_type_fn(arena, necro_ast_create_conid(arena, intern, "World", NECRO_CON_TYPE_VAR), necro_ast_create_conid(arena, intern, "Int", NECRO_CON_TYPE_VAR)),
            NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        necro_append_top(arena, top,
            necro_ast_create_apats_assignment(arena, intern, "getMouseX",
                necro_ast_create_apats(arena, necro_ast_create_var(arena, intern, "w", NECRO_VAR_DECLARATION), NULL),
                necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "_primUndefined", NECRO_VAR_VAR), NULL)));
    }

    // mouseY
    {
        necro_append_top(arena, top, necro_ast_create_fn_type_sig(arena, intern, "getMouseY", NULL,
            necro_ast_create_type_fn(arena, necro_ast_create_conid(arena, intern, "World", NECRO_CON_TYPE_VAR), necro_ast_create_conid(arena, intern, "Int", NECRO_CON_TYPE_VAR)),
            NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        necro_append_top(arena, top,
            necro_ast_create_apats_assignment(arena, intern, "getMouseY",
                necro_ast_create_apats(arena, necro_ast_create_var(arena, intern, "w", NECRO_VAR_DECLARATION), NULL),
                necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "_primUndefined", NECRO_VAR_VAR), NULL)));
    }

    // unsafeMalloc
    {
        necro_append_top(arena, top, necro_ast_create_fn_type_sig(arena, intern, "unsafeMalloc", NULL,
            necro_ast_create_type_fn(arena, necro_ast_create_conid(arena, intern, "Int", NECRO_CON_TYPE_VAR),
                necro_ast_create_type_app(arena,
                    necro_ast_create_conid(arena, intern, "Ptr", NECRO_CON_TYPE_VAR),
                    necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR))
                ),
            NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        necro_append_top(arena, top,
            necro_ast_create_apats_assignment(arena, intern, "unsafeMalloc",
                necro_ast_create_apats(arena, necro_ast_create_var(arena, intern, "size", NECRO_VAR_DECLARATION), NULL),
                necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "_primUndefined", NECRO_VAR_VAR), NULL)));
    }

    // unsafePeek
    {
        necro_append_top(arena, top, necro_ast_create_fn_type_sig(arena, intern, "unsafePeek", NULL,
            necro_ast_create_type_fn(arena,
                necro_ast_create_conid(arena, intern, "Int", NECRO_CON_TYPE_VAR),
                necro_ast_create_type_fn(arena,
                    necro_ast_create_type_app(arena,
                        necro_ast_create_conid(arena, intern, "Ptr", NECRO_CON_TYPE_VAR),
                        necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR)),
                    necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR))
                ),
            NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        necro_append_top(arena, top,
            necro_ast_create_apats_assignment(arena, intern, "unsafePeek",
                necro_ast_create_apats(arena, necro_ast_create_var(arena, intern, "index", NECRO_VAR_DECLARATION), necro_ast_create_apats(arena, necro_ast_create_var(arena, intern, "ptr", NECRO_VAR_DECLARATION), NULL)),
                necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "_primUndefined", NECRO_VAR_VAR), NULL)));
    }

    // unsafePoke
    {
        necro_append_top(arena, top, necro_ast_create_fn_type_sig(arena, intern, "unsafePoke", NULL,
            necro_ast_create_type_fn(arena,
                necro_ast_create_conid(arena, intern, "Int", NECRO_CON_TYPE_VAR),
                necro_ast_create_type_fn(arena,
                    necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR),
                    necro_ast_create_type_fn(arena,
                        necro_ast_create_type_app(arena,
                            necro_ast_create_conid(arena, intern, "Ptr", NECRO_CON_TYPE_VAR),
                            necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR)),
                        necro_ast_create_type_app(arena,
                            necro_ast_create_conid(arena, intern, "Ptr", NECRO_CON_TYPE_VAR),
                            necro_ast_create_var(arena, intern, "a", NECRO_VAR_TYPE_FREE_VAR))))),
            NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        necro_append_top(arena, top,
            necro_ast_create_apats_assignment(arena, intern, "unsafePoke",
                necro_ast_create_apats(arena, necro_ast_create_var(arena, intern, "index", NECRO_VAR_DECLARATION), necro_ast_create_apats(arena, necro_ast_create_var(arena, intern, "value", NECRO_VAR_DECLARATION), necro_ast_create_apats(arena, necro_ast_create_var(arena, intern, "ptr", NECRO_VAR_DECLARATION), NULL))),
                necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "_primUndefined", NECRO_VAR_VAR), NULL)));
    }

    // plusPtr??

    // && / ||
    {
        NecroAst* bool_conid = necro_ast_create_conid(arena, intern, "Bool", NECRO_CON_TYPE_VAR);
        necro_append_top(arena, top,
            necro_ast_create_fn_type_sig(arena, intern, "&&", NULL,
                necro_ast_create_type_fn(arena,
                    bool_conid,
                    necro_ast_create_type_fn(arena,
                        bool_conid,
                        bool_conid)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "&&", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "_primUndefined", NECRO_VAR_VAR), NULL)));
        necro_append_top(arena, top,
            necro_ast_create_fn_type_sig(arena, intern, "||", NULL,
                necro_ast_create_type_fn(arena,
                    bool_conid,
                    necro_ast_create_type_fn(arena,
                        bool_conid,
                        bool_conid)), NECRO_VAR_SIG, NECRO_SIG_DECLARATION));
        necro_append_top(arena, top, necro_ast_create_simple_assignment(arena, intern, "||", necro_ast_create_rhs(arena, necro_ast_create_var(arena, intern, "_primUndefined", NECRO_VAR_VAR), NULL)));
    }

    // Compile, part I
    necro_build_scopes(info, scoped_symtable, &base.ast);
    unwrap(void, necro_rename(info, scoped_symtable, intern, &base.ast));
    necro_dependency_analyze(info, intern, &base.ast);

    // Cache useful symbols
    base.prim_undefined         = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "_primUndefined"));;

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

    base.poly_type              = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "_Poly"));
    base.world_type             = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "World"));
    base.world_value            = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "world"));
    base.unit_type              = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "()"));
    base.unit_con               = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "()"));
    // base.list_type              = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "[]"));
    base.int_type               = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Int"));
    base.float_type             = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Float"));
    base.audio_type             = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Audio"));
    base.rational_type          = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Rational"));
    base.char_type              = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Char"));
    base.bool_type              = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Bool"));
    base.num_type_class         = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Num"));
    base.fractional_type_class  = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Fractional"));
    base.eq_type_class          = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Eq"));
    base.ord_type_class         = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Ord"));
    base.functor_type_class     = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Functor"));
    base.applicative_type_class = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Applicative"));
    base.monad_type_class       = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Monad"));
    base.default_type_class     = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Default"));
    base.event_type             = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Event"));
    base.pattern_type           = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Pattern"));
    // base.closure_type           = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "_Closure"));
    // base.apply_fn               = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "_apply"));
    // base.dyn_state_type         = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "_DynState"));
    base.ptr_type               = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Ptr"));
    base.array_type             = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Array"));
    base.maybe_type             = necro_symtable_get_type_ast_symbol(scoped_symtable, necro_intern_string(intern, "Maybe"));;

    // Runtime functions
    base.mouse_x_fn             = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "getMouseX"));
    base.mouse_y_fn             = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "getMouseY"));
    base.unsafe_malloc          = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "unsafeMalloc"));
    base.unsafe_peek            = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "unsafePeek"));
    base.unsafe_poke            = necro_symtable_get_top_level_ast_symbol(scoped_symtable, necro_intern_string(intern, "unsafePoke"));

    // Compile, part II
    unwrap(void, necro_infer(info, intern, scoped_symtable, &base, &base.ast));
    unwrap(void, necro_monomorphize(info, intern, scoped_symtable, &base, &base.ast));

    // Finish
    return base;
}

#define NECRO_BASE_TEST_VERBOSE 0

void necro_base_test()
{
    necro_announce_phase("NecroBase");

    // Set up
    NecroIntern         intern          = necro_intern_create();
    NecroAstArena       ast             = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
    NecroSymTable       symtable        = necro_symtable_create(&intern);
    NecroScopedSymTable scoped_symtable = necro_scoped_symtable_create(&symtable);
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
    necro_symtable_destroy(&symtable);
    necro_intern_destroy(&intern);
}

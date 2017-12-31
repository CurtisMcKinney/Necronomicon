/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include "infer.h"
#include "prim.h"

//=====================================================
// Forward Declarations
//=====================================================
void necro_init_prim_defs(NecroPrimTypes* prim_types, NecroIntern* intern);
void necro_add_prim_symbol_info(NecroPrimTypes* prim_types, NecroScopedSymTable* scoped_symtable);
void necro_add_prim_types(NecroPrimTypes* prim_types, NecroInfer* infer);

//=====================================================
// Symbols
//=====================================================
NecroPrimTypes necro_create_prim_types(NecroIntern* intern)
{
    // BinOp
    NecroBinOpTypes bin_op_types = (NecroBinOpTypes)
    {
        // .add_type                = (NecroCon) { .symbol = necro_intern_string(intern, "+"),   .id = { 0 } },
        // .sub_type                = (NecroCon) { .symbol = necro_intern_string(intern, "-"),   .id = { 0 } },
        // .mul_type                = (NecroCon) { .symbol = necro_intern_string(intern, "*"),   .id = { 0 } },
        // .div_type                = (NecroCon) { .symbol = necro_intern_string(intern, "/"),   .id = { 0 } },
        // .eq_type                 = (NecroCon) { .symbol = necro_intern_string(intern, "=="),  .id = { 0 } },
        // .not_eq_type             = (NecroCon) { .symbol = necro_intern_string(intern, "/="),  .id = { 0 } },
        // .gt_type                 = (NecroCon) { .symbol = necro_intern_string(intern, ">"),   .id = { 0 } },
        // .lt_type                 = (NecroCon) { .symbol = necro_intern_string(intern, "<"),   .id = { 0 } },
        // .gte_type                = (NecroCon) { .symbol = necro_intern_string(intern, "=>"),  .id = { 0 } },
        // .lte_type                = (NecroCon) { .symbol = necro_intern_string(intern, "<="),  .id = { 0 } },
        .mod_type                = (NecroCon) { .symbol = necro_intern_string(intern, "%"),   .id = { 0 } },
        .and_type                = (NecroCon) { .symbol = necro_intern_string(intern, "&&"),  .id = { 0 } },
        .or_type                 = (NecroCon) { .symbol = necro_intern_string(intern, "||"),  .id = { 0 } },
        .double_colon_type       = (NecroCon) { .symbol = necro_intern_string(intern, "::"),  .id = { 0 } },
        .left_shift_type         = (NecroCon) { .symbol = necro_intern_string(intern, "<<"),  .id = { 0 } },
        .right_shift_type        = (NecroCon) { .symbol = necro_intern_string(intern, ">>"),  .id = { 0 } },
        .pipe_type               = (NecroCon) { .symbol = necro_intern_string(intern, "|"),   .id = { 0 } },
        .forward_pipe_type       = (NecroCon) { .symbol = necro_intern_string(intern, "|>"),  .id = { 0 } },
        .back_pipe_type          = (NecroCon) { .symbol = necro_intern_string(intern, "<|"),  .id = { 0 } },
        .dot_type                = (NecroCon) { .symbol = necro_intern_string(intern, "."),   .id = { 0 } },
        .bind_right_type         = (NecroCon) { .symbol = necro_intern_string(intern, ">>="), .id = { 0 } },
        .bind_left_type          = (NecroCon) { .symbol = necro_intern_string(intern, "=<<"), .id = { 0 } },
        .double_exclamation_type = (NecroCon) { .symbol = necro_intern_string(intern, "!!"),  .id = { 0 } },
        .append_type             = (NecroCon) { .symbol = necro_intern_string(intern, "++"),  .id = { 0 } },
    };

    // // Tuple
    // NecroTupleTypes tuple_types = (NecroTupleTypes)
    // {
    //     .two   = (NecroCon) { .symbol = necro_intern_string(intern, "(,)"), .id = { 0 } },
    //     .three = (NecroCon) { .symbol = necro_intern_string(intern, "(,,)"), .id = { 0 } },
    //     .four  = (NecroCon) { .symbol = necro_intern_string(intern, "(,,,)"), .id = { 0 } },
    //     .five  = (NecroCon) { .symbol = necro_intern_string(intern, "(,,,,)"), .id = { 0 } },
    //     .six   = (NecroCon) { .symbol = necro_intern_string(intern, "(,,,,,)"), .id = { 0 } },
    //     .seven = (NecroCon) { .symbol = necro_intern_string(intern, "(,,,,,,)"), .id = { 0 } },
    //     .eight = (NecroCon) { .symbol = necro_intern_string(intern, "(,,,,,,,)"), .id = { 0 } },
    //     .nine  = (NecroCon) { .symbol = necro_intern_string(intern, "(,,,,,,,,)"), .id = { 0 } },
    //     .ten   = (NecroCon) { .symbol = necro_intern_string(intern, "(,,,,,,,,,)"), .id = { 0 } },
    // };

    // PrimSymbols
    return (NecroPrimTypes)
    {
        .bin_op_types   = bin_op_types,
        // .tuple_types    = tuple_types,
        // .io_type        = (NecroCon) { .symbol = necro_intern_string(intern, "IO"),    .id = { 0 } },
        // .list_type      = (NecroCon) { .symbol = necro_intern_string(intern, "[]"),    .id = { 0 } },
        // .unit_type      = (NecroCon) { .symbol = necro_intern_string(intern, "()"),    .id = { 0 } },
        // .int_type       = (NecroCon) { .symbol = necro_intern_string(intern, "Int"),   .id = { 0 } },
        // .float_type     = (NecroCon) { .symbol = necro_intern_string(intern, "Float"), .id = { 0 } },
        // .audio_type     = (NecroCon) { .symbol = necro_intern_string(intern, "Audio"), .id = { 0 } },
        // .char_type      = (NecroCon) { .symbol = necro_intern_string(intern, "Char"),  .id = { 0 } },
        // .bool_type      = (NecroCon) { .symbol = necro_intern_string(intern, "Bool"),  .id = { 0 } },
        // .type_def_table = necro_create_prim_def_table(),
        // .val_def_table  = necro_create_prim_def_table(),
        .arena          = necro_create_paged_arena(),
        .defs           = NULL,
        .def_head       = NULL
    };
}

// //=====================================================
// // SymbolInfo and IDs
// //=====================================================
// inline void necro_add_bin_op_prim_symbol_info(NecroScopedSymTable* scoped_symtable, NecroCon* conid)
// {
//     NecroSymbolInfo symbol_info = (NecroSymbolInfo)
//     {
//         .name       = conid->symbol,
//         .data_size  = 2,
//         .source_loc = (NecroSourceLoc) {0, 0, 0},
//         .scope      = scoped_symtable->top_scope,
//     };
//     conid->id = necro_scoped_symtable_new_symbol_info(scoped_symtable, scoped_symtable->top_scope, symbol_info);
// }

// inline void necro_add_tuple_symbol_info(NecroScopedSymTable* scoped_symtable, NecroCon* conid, size_t data_size)
// {
//     NecroSymbolInfo symbol_info = (NecroSymbolInfo)
//     {
//         .name       = conid->symbol,
//         .data_size  = data_size,
//         .source_loc = (NecroSourceLoc) {0, 0, 0},
//         .scope      = scoped_symtable->top_scope,
//     };
//     conid->id = necro_scoped_symtable_new_symbol_info(scoped_symtable, scoped_symtable->top_scope, symbol_info);
// }

// inline void necro_add_type_symbol_info(NecroScopedSymTable* scoped_symtable, NecroCon* conid, size_t data_size)
// {
//     NecroSymbolInfo symbol_info = (NecroSymbolInfo)
//     {
//         .name       = conid->symbol,
//         .data_size  = data_size,
//         .source_loc = (NecroSourceLoc) {0, 0, 0},
//         .scope      = scoped_symtable->top_type_scope,
//     };
//     conid->id = necro_scoped_symtable_new_symbol_info(scoped_symtable, scoped_symtable->top_type_scope, symbol_info);
//     assert(conid->id.id != 0);
// }

// inline void necro_add_constructor_symbol_info(NecroScopedSymTable* scoped_symtable, NecroCon* conid, size_t data_size)
// {
//     NecroSymbolInfo symbol_info = (NecroSymbolInfo)
//     {
//         .name       = conid->symbol,
//         .data_size  = data_size,
//         .source_loc = (NecroSourceLoc) {0, 0, 0},
//         .scope      = scoped_symtable->top_scope,
//     };
//     conid->id = necro_scoped_symtable_new_symbol_info(scoped_symtable, scoped_symtable->top_scope, symbol_info);
// }

// void necro_add_prim_type_symbol_info(NecroPrimTypes* prim_types, NecroScopedSymTable* scoped_symtable)
// {
//     return;
//     // Prim Defs
//     necro_init_prim_defs(prim_types, scoped_symtable->global_table->intern);
//     necro_add_prim_symbol_info(prim_types, scoped_symtable);

//     // // Bin Ops
//     // necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.add_type);
//     // necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.sub_type);
//     // necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.mul_type);
//     // necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.div_type);
//     // necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.mod_type);
//     // necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.eq_type);
//     // necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.not_eq_type);
//     // necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.gt_type);
//     // necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.lt_type);
//     // necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.gte_type);
//     // necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.lte_type);
//     // necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.and_type);
//     // necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.or_type);
//     // necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.double_colon_type);
//     // necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.left_shift_type);
//     // necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.right_shift_type);
//     // necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.pipe_type);
//     // necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.forward_pipe_type);
//     // necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.back_pipe_type);
//     // necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.dot_type);
//     // necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.bind_right_type);
//     // necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.bind_left_type);
//     // necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.double_exclamation_type);
//     // necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.append_type);

//     // Tuples Constructors
//     // necro_add_tuple_symbol_info(scoped_symtable, &prim_types->tuple_types.two,   2);
//     // necro_add_tuple_symbol_info(scoped_symtable, &prim_types->tuple_types.three, 3);
//     // necro_add_tuple_symbol_info(scoped_symtable, &prim_types->tuple_types.four,  4);
//     // necro_add_tuple_symbol_info(scoped_symtable, &prim_types->tuple_types.five,  5);
//     // necro_add_tuple_symbol_info(scoped_symtable, &prim_types->tuple_types.six,   6);
//     // necro_add_tuple_symbol_info(scoped_symtable, &prim_types->tuple_types.seven, 7);
//     // necro_add_tuple_symbol_info(scoped_symtable, &prim_types->tuple_types.eight, 8);
//     // necro_add_tuple_symbol_info(scoped_symtable, &prim_types->tuple_types.nine,  9);
//     // necro_add_tuple_symbol_info(scoped_symtable, &prim_types->tuple_types.ten,   10);

//     // Tuples Types
//     // necro_add_type_symbol_info(scoped_symtable, &prim_types->tuple_types.two,   2);
//     // necro_add_type_symbol_info(scoped_symtable, &prim_types->tuple_types.three, 3);
//     // necro_add_type_symbol_info(scoped_symtable, &prim_types->tuple_types.four,  4);
//     // necro_add_type_symbol_info(scoped_symtable, &prim_types->tuple_types.five,  5);
//     // necro_add_type_symbol_info(scoped_symtable, &prim_types->tuple_types.six,   6);
//     // necro_add_type_symbol_info(scoped_symtable, &prim_types->tuple_types.seven, 7);
//     // necro_add_type_symbol_info(scoped_symtable, &prim_types->tuple_types.eight, 8);
//     // necro_add_type_symbol_info(scoped_symtable, &prim_types->tuple_types.nine,  9);
//     // necro_add_type_symbol_info(scoped_symtable, &prim_types->tuple_types.ten,   10);

//     // Types
//     // necro_add_type_symbol_info(scoped_symtable, &prim_types->io_type,    1);
//     // necro_add_type_symbol_info(scoped_symtable, &prim_types->list_type,  1);

//     // necro_add_type_symbol_info(scoped_symtable, &prim_types->int_type,   1);
//     // necro_add_type_symbol_info(scoped_symtable, &prim_types->float_type, 1);
//     // necro_add_type_symbol_info(scoped_symtable, &prim_types->audio_type, 0);
//     // necro_add_type_symbol_info(scoped_symtable, &prim_types->char_type,  1);

//     // necro_add_type_symbol_info(scoped_symtable, &prim_types->bool_type,  1);
//     // necro_add_type_symbol_info(scoped_symtable, &prim_types->unit_type,  0);

//     // Constructors
//     // necro_add_constructor_symbol_info(scoped_symtable, &prim_types->unit_type, 0);
// }

//=====================================================
// Type Sigs
//=====================================================
// void necro_add_bin_op_prim_type_sig(NecroInfer* infer, NecroCon conid, NecroType* left_type, NecroType* right_type, NecroType* result_type)
// {
//     assert(conid.id.id < infer->symtable->count);
//     necro_symtable_get(infer->symtable, conid.id)->type               = necro_create_type_fun(infer, left_type, necro_create_type_fun(infer, right_type, result_type));
//     necro_symtable_get(infer->symtable, conid.id)->type->pre_supplied = true;
// }

// void necro_add_prim_type_sigs(NecroPrimTypes* prim_types, NecroInfer* infer)
// {
//     return;
//     necro_add_prim_types(prim_types, infer);

//     // declare types
//     // NecroType* int_type  = necro_declare_type(infer, prim_types->int_type,  0);

//     // // TODO: Replace
//     // NecroType* bool_type = necro_declare_type(infer, prim_types->bool_type, 0);
//     // necro_declare_type(infer, prim_types->unit_type,  0);
//     // necro_declare_type(infer, prim_types->float_type, 0);
//     // necro_declare_type(infer, prim_types->char_type,  0);
//     // necro_declare_type(infer, prim_types->audio_type, 0);

//     // necro_declare_type(infer, prim_types->list_type,  1);
//     // necro_declare_type(infer, prim_types->io_type,    1);

//     // necro_declare_type(infer, prim_types->tuple_types.two,   2);
//     // necro_declare_type(infer, prim_types->tuple_types.three, 3);
//     // necro_declare_type(infer, prim_types->tuple_types.four,  4);
//     // necro_declare_type(infer, prim_types->tuple_types.five,  5);
//     // necro_declare_type(infer, prim_types->tuple_types.six,   6);
//     // necro_declare_type(infer, prim_types->tuple_types.seven, 7);
//     // necro_declare_type(infer, prim_types->tuple_types.eight, 8);
//     // necro_declare_type(infer, prim_types->tuple_types.nine,  9);
//     // necro_declare_type(infer, prim_types->tuple_types.ten,   10);

//     // // Old
//     // necro_add_bin_op_prim_type_sig(infer, prim_types->bin_op_types.add_type, int_type, int_type, int_type);
//     // necro_add_bin_op_prim_type_sig(infer, prim_types->bin_op_types.sub_type, int_type, int_type, int_type);
//     // necro_add_bin_op_prim_type_sig(infer, prim_types->bin_op_types.mul_type, int_type, int_type, int_type);
//     // necro_add_bin_op_prim_type_sig(infer, prim_types->bin_op_types.div_type, int_type, int_type, int_type);
//     // necro_add_bin_op_prim_type_sig(infer, prim_types->bin_op_types.mod_type, int_type, int_type, int_type);

//     // necro_add_bin_op_prim_type_sig(infer, prim_types->bin_op_types.eq_type, int_type, int_type, bool_type);
//     // necro_add_bin_op_prim_type_sig(infer, prim_types->bin_op_types.not_eq_type, int_type, int_type, bool_type);
//     // necro_add_bin_op_prim_type_sig(infer, prim_types->bin_op_types.gt_type, int_type, int_type, bool_type);
//     // necro_add_bin_op_prim_type_sig(infer, prim_types->bin_op_types.lt_type, int_type, int_type, bool_type);
//     // necro_add_bin_op_prim_type_sig(infer, prim_types->bin_op_types.gte_type, int_type, int_type, bool_type);
//     // necro_add_bin_op_prim_type_sig(infer, prim_types->bin_op_types.lte_type, int_type, int_type, bool_type);
//     // necro_add_bin_op_prim_type_sig(infer, prim_types->bin_op_types.and_type, bool_type, bool_type, bool_type);
//     // necro_add_bin_op_prim_type_sig(infer, prim_types->bin_op_types.or_type, bool_type, bool_type, bool_type);

//     // TODO: Finish other operators
//     // necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.bind_right_type);
//     // necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.bind_left_type);
//     // necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.double_exclamation_type);
//     // necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.append_type);
//     // necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.dot_type);
//     // necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.back_pipe_type);
//     // necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.forward_pipe_type);
//     // necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.pipe_type);
//     // necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.left_shift_type);
//     // necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.right_shift_type);
//     // necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.double_colon_type);
// }

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
    // assert(data_declaration_ast->data_declaration.constructor_list != NULL);
    // assert(data_declaration_ast->data_declaration.constructor_list->type == NECRO_AST_LIST_NODE);
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
    // class_def->name                     = ;
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
    NecroSymbolInfo fun_symbol_info = (NecroSymbolInfo)
    {
        .name       = fun_symbol,
        .data_size  = 0,
        .source_loc = (NecroSourceLoc) {0, 0, 0},
        .scope      = NULL,
    };
    fun_def->name                 = fun_name;
    fun_def->fun_def.symbol_info  = fun_symbol_info;
    fun_def->fun_def.type         = NULL;
    fun_def->fun_def.type_sig_ast = type_sig_ast;
    return fun_def;
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
        case NECRO_PRIM_DEF_FUN:
        {
            necro_build_scopes_go(scoped_symtable, def->fun_def.type_sig_ast);
            break;
        }
        case NECRO_PRIM_DEF_INSTANCE:
        {
            necro_build_scopes_go(scoped_symtable, def->instance_def.instance_ast);
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
    NecroPrimDef* def = prim_types->def_head;
    while (def != NULL)
    {
        switch (def->type)
        {
        case NECRO_PRIM_DEF_DATA:
        {
            necro_rename_declare_pass(renamer, def->data_def.data_declaration_ast);
            necro_rename_var_pass(renamer, def->data_def.data_declaration_ast);
            if (def->global_name != NULL)
            {
                def->global_name->symbol = def->data_def.data_declaration_ast->data_declaration.simpletype->simple_type.type_con->conid.symbol;
                def->global_name->id     = def->data_def.data_declaration_ast->data_declaration.simpletype->simple_type.type_con->conid.id;
            }
            break;
        }
        case NECRO_PRIM_DEF_CLASS:
        {
            necro_rename_declare_pass(renamer, def->class_def.type_class_ast);
            necro_rename_var_pass(renamer, def->class_def.type_class_ast);
            if (def->global_name != NULL)
            {
                def->global_name->symbol = def->class_def.type_class_ast->type_class_declaration.tycls->conid.symbol;
                def->global_name->id     = def->class_def.type_class_ast->type_class_declaration.tycls->conid.id;
            }
            break;
        }
        case NECRO_PRIM_DEF_FUN:
        {
            necro_rename_declare_pass(renamer, def->fun_def.type_sig_ast);
            necro_rename_var_pass(renamer, def->fun_def.type_sig_ast);
            if (def->global_name != NULL)
            {
                def->global_name->symbol = def->fun_def.type_sig_ast->type_signature.var->variable.symbol;
                def->global_name->id     = def->fun_def.type_sig_ast->type_signature.var->variable.id;
            }
            break;
        }
        case NECRO_PRIM_DEF_INSTANCE:
        {
            necro_rename_declare_pass(renamer, def->instance_def.instance_ast);
            necro_rename_var_pass(renamer, def->instance_def.instance_ast);
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

NECRO_RETURN_CODE necro_prim_infer(NecroPrimTypes* prim_types, NecroInfer* infer)
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
        default: assert(false); break;
        }
        def = def->next;
    }
    // Run type checking on the whole top level declarations
    necro_infer_go(infer, head);

    // Is there a better solution for bin ops than this?
    // Num Bin Op types
    NecroType* num_new_name           = necro_new_name(infer, (NecroSourceLoc) { 0, 0, 0 });
    num_new_name->var.context         = necro_create_type_class_context(&infer->arena, infer->prim_types->num_type_class, (NecroCon) { .id = num_new_name->var.var.id, .symbol = num_new_name->var.var.symbol }, NULL);
    NecroType* num_bin_op_type        = necro_create_type_fun(infer, num_new_name, necro_create_type_fun(infer, num_new_name, num_new_name));
    num_bin_op_type                   = necro_gen(infer, num_bin_op_type, NULL);
    prim_types->bin_op_types.add_type = num_bin_op_type;
    prim_types->bin_op_types.sub_type = num_bin_op_type;
    prim_types->bin_op_types.mul_type = num_bin_op_type;

    // Fractional Bin Op types
    NecroType* frac_new_name          = necro_new_name(infer, (NecroSourceLoc) { 0, 0, 0 });
    frac_new_name->var.context        = necro_create_type_class_context(&infer->arena, infer->prim_types->fractional_type_class, (NecroCon) { .id = frac_new_name->var.var.id, .symbol = frac_new_name->var.var.symbol }, NULL);
    NecroType* frac_bin_op_type       = necro_create_type_fun(infer, frac_new_name, necro_create_type_fun(infer, frac_new_name, frac_new_name));
    frac_bin_op_type                  = necro_gen(infer, frac_bin_op_type, NULL);
    prim_types->bin_op_types.div_type = frac_bin_op_type;

    // Eq Bin Op types
    NecroType* eq_new_name               = necro_new_name(infer, (NecroSourceLoc) { 0, 0, 0 });
    eq_new_name->var.context             = necro_create_type_class_context(&infer->arena, infer->prim_types->eq_type_class, (NecroCon) { .id = eq_new_name->var.var.id, .symbol = eq_new_name->var.var.symbol }, NULL);
    NecroType* eq_bin_op_type            = necro_create_type_fun(infer, eq_new_name, necro_create_type_fun(infer, eq_new_name, necro_symtable_get(infer->symtable, prim_types->bool_type.id)->type));
    eq_bin_op_type                       = necro_gen(infer, eq_bin_op_type, NULL);
    prim_types->bin_op_types.eq_type     = eq_bin_op_type;
    prim_types->bin_op_types.not_eq_type = eq_bin_op_type;

    // Ord Bin Op types
    NecroType* ord_new_name              = necro_new_name(infer, (NecroSourceLoc) { 0, 0, 0 });
    ord_new_name->var.context            = necro_create_type_class_context(&infer->arena, infer->prim_types->ord_type_class, (NecroCon) { .id = ord_new_name->var.var.id, .symbol = ord_new_name->var.var.symbol }, NULL);
    NecroType* ord_bin_op_type           = necro_create_type_fun(infer, ord_new_name, necro_create_type_fun(infer, ord_new_name, necro_symtable_get(infer->symtable, prim_types->bool_type.id)->type));
    ord_bin_op_type                      = necro_gen(infer, ord_bin_op_type, NULL);
    prim_types->bin_op_types.lte_type    = ord_bin_op_type;
    prim_types->bin_op_types.lt_type     = ord_bin_op_type;
    prim_types->bin_op_types.gte_type    = ord_bin_op_type;
    prim_types->bin_op_types.gt_type     = ord_bin_op_type;

    // Cons Operator
    NecroType* name      = necro_new_name(infer, (NecroSourceLoc) { 0, 0, 0 });
    NecroType* list_type = necro_make_con_1(infer, prim_types->list_type, name);
    prim_types->bin_op_types.cons_type = necro_gen(infer, necro_create_type_fun(infer, name, necro_create_type_fun(infer, list_type, list_type)), NULL);

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

void necro_create_prim_num_instances(NecroPrimTypes* prim_types, NecroIntern* intern, const char* data_type_name, bool is_fractional)
{
    NecroASTNode* type_conid = necro_create_conid_ast(&prim_types->arena, intern, data_type_name, NECRO_CON_TYPE_VAR);
    NecroASTNode* bool_conid = necro_create_conid_ast(&prim_types->arena, intern, "Bool", NECRO_CON_TYPE_VAR);

    //--------------
    // Num
    NecroASTNode* bin_math_ast      = necro_create_fun_ast(&prim_types->arena, type_conid, necro_create_fun_ast(&prim_types->arena, type_conid, type_conid));
    NecroASTNode* unary_math_ast    = necro_create_fun_ast(&prim_types->arena, type_conid, type_conid);

    const char*   add_symbol        = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primAdd@"), type_conid->conid.symbol));
    NecroPrimDef* add_type_def      = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, add_symbol, NULL, bin_math_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    const char*   sub_symbol        = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primSub@"), type_conid->conid.symbol));
    NecroPrimDef* sub_type_def      = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, sub_symbol, NULL, bin_math_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    const char*   mul_symbol        = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primMul@"), type_conid->conid.symbol));
    NecroPrimDef* mul_type_def      = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, mul_symbol, NULL, bin_math_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    const char*   abs_symbol        = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primAbs@"), type_conid->conid.symbol));
    NecroPrimDef* abs_type_def      = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, abs_symbol, NULL, unary_math_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    const char*   signum_symbol     = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primSignum@"), type_conid->conid.symbol));
    NecroPrimDef* signum_type_def   = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, signum_symbol, NULL, unary_math_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    const char*   from_int_symbol   = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primFromInt@"), type_conid->conid.symbol));
    NecroASTNode* from_int_ast      = necro_create_fun_ast(&prim_types->arena, necro_create_conid_ast(&prim_types->arena, intern, "Int", NECRO_CON_TYPE_VAR), type_conid);
    NecroPrimDef* from_int_type_def = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, from_int_symbol, NULL, from_int_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    NecroASTNode* num_method_list  =
        necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "add", necro_create_variable_ast(&prim_types->arena, intern, add_symbol, NECRO_VAR_VAR)),
            necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "sub", necro_create_variable_ast(&prim_types->arena, intern, sub_symbol, NECRO_VAR_VAR)),
                necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "mul", necro_create_variable_ast(&prim_types->arena, intern, mul_symbol, NECRO_VAR_VAR)),
                    necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "abs", necro_create_variable_ast(&prim_types->arena, intern, abs_symbol, NECRO_VAR_VAR)),
                        necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "signum", necro_create_variable_ast(&prim_types->arena, intern, signum_symbol, NECRO_VAR_VAR)),
                            necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "fromInt", necro_create_variable_ast(&prim_types->arena, intern, from_int_symbol, NECRO_VAR_VAR)), NULL))))));
    NecroASTNode* num_instance_ast = necro_create_instance_ast(&prim_types->arena, intern, "Num", type_conid, NULL, num_method_list);
    NecroPrimDef* num_instance_def = necro_prim_def_instance(prim_types, intern, num_instance_ast);

    //--------------
    // Fractional
    if (is_fractional)
    {
        const char*   div_symbol     = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primDiv@"), type_conid->conid.symbol));
        NecroPrimDef* div_type_def   = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, div_symbol, NULL, bin_math_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

        const char*   recip_symbol   = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primRecip@"), type_conid->conid.symbol));
        NecroPrimDef* recip_type_def = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, recip_symbol, NULL, unary_math_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

        const char*   from_rational_symbol   = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primFromRational@"), type_conid->conid.symbol));
        NecroASTNode* from_rational_ast      = necro_create_fun_ast(&prim_types->arena, necro_create_conid_ast(&prim_types->arena, intern, "Rational", NECRO_CON_TYPE_VAR), type_conid);
        NecroPrimDef* from_rational_type_def = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, from_rational_symbol, NULL, from_rational_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

        NecroASTNode* fractional_method_list =
            necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "div", necro_create_variable_ast(&prim_types->arena, intern, div_symbol, NECRO_VAR_VAR)),
                necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "recip", necro_create_variable_ast(&prim_types->arena, intern, recip_symbol, NECRO_VAR_VAR)),
                    necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "fromRational", necro_create_variable_ast(&prim_types->arena, intern, from_rational_symbol, NECRO_VAR_VAR)), NULL)));
        NecroASTNode* fractional_instance_ast = necro_create_instance_ast(&prim_types->arena, intern, "Fractional", type_conid, NULL, fractional_method_list);
        NecroPrimDef* fractional_instance_def = necro_prim_def_instance(prim_types, intern, fractional_instance_ast);
    }

    //--------------
    // Eq
    NecroASTNode* bin_comp_ast = necro_create_fun_ast(&prim_types->arena, type_conid, necro_create_fun_ast(&prim_types->arena, type_conid, bool_conid));
    const char*   eq_symbol    = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primEq@"), type_conid->conid.symbol));
    NecroPrimDef* eq_type_def  = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, eq_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    const char*   neq_symbol   = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primNEq@"), type_conid->conid.symbol));
    NecroPrimDef* neq_type_def = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, neq_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    NecroASTNode* eq_method_list =
        necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "eq", necro_create_variable_ast(&prim_types->arena, intern, eq_symbol, NECRO_VAR_VAR)),
            necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "neq", necro_create_variable_ast(&prim_types->arena, intern, neq_symbol, NECRO_VAR_VAR)), NULL));
    NecroASTNode* eq_instance_ast = necro_create_instance_ast(&prim_types->arena, intern, "Eq", type_conid, NULL, eq_method_list);
    NecroPrimDef* eq_instance_def = necro_prim_def_instance(prim_types, intern, eq_instance_ast);

    //--------------
    // Ord
    const char*   gt_symbol    = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primGT@"), type_conid->conid.symbol));
    NecroPrimDef* gt_type_def  = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, gt_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    const char*   lt_symbol    = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primLT@"), type_conid->conid.symbol));
    NecroPrimDef* lt_type_def  = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, lt_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    const char*   gte_symbol   = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primGTE@"), type_conid->conid.symbol));
    NecroPrimDef* gte_type_def = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, gte_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    const char*   lte_symbol   = necro_intern_get_string(intern, necro_intern_concat_symbols(intern, necro_intern_string(intern, "primLTE@"), type_conid->conid.symbol));
    NecroPrimDef* lte_type_def = necro_prim_def_fun(prim_types, intern, NULL, necro_create_fun_type_sig_ast(&prim_types->arena, intern, lte_symbol, NULL, bin_comp_ast, NECRO_VAR_DECLARATION /*NECRO_VAR_SIG don't use this because prim funs have no body*/, NECRO_SIG_DECLARATION));

    NecroASTNode* ord_method_list =
        necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "lt", necro_create_variable_ast(&prim_types->arena, intern, lt_symbol, NECRO_VAR_VAR)),
            necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "gt", necro_create_variable_ast(&prim_types->arena, intern, gt_symbol, NECRO_VAR_VAR)),
                necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "lte", necro_create_variable_ast(&prim_types->arena, intern, lte_symbol, NECRO_VAR_VAR)),
                    necro_create_declaration_list(&prim_types->arena, necro_create_simple_assignment(&prim_types->arena, intern, "gte", necro_create_variable_ast(&prim_types->arena, intern, gte_symbol, NECRO_VAR_VAR)), NULL))));
    NecroASTNode* ord_instance_ast = necro_create_instance_ast(&prim_types->arena, intern, "Ord", type_conid, NULL, ord_method_list);
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

void necro_init_prim_defs(NecroPrimTypes* prim_types, NecroIntern* intern)
{
    // Primitive Rational type?

    // TODO: () isn't printing in type sigs for some reason?!?!?!

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

    // IO
    NecroASTNode* io_s_type   = necro_create_simple_type_ast(&prim_types->arena, intern, "IO", necro_create_var_list_ast(&prim_types->arena, intern, 1, NECRO_VAR_TYPE_VAR_DECLARATION));
    NecroPrimDef* io_data_def = necro_prim_def_data(prim_types, intern, &prim_types->io_type, necro_create_data_declaration_ast(&prim_types->arena, intern, io_s_type, NULL));

    // Bool
    NecroASTNode* bool_s_type           = necro_create_simple_type_ast(&prim_types->arena, intern, "Bool", NULL);
    NecroASTNode* true_constructor      = necro_create_data_constructor_ast(&prim_types->arena, intern, "True", NULL);
    NecroASTNode* false_constructor     = necro_create_data_constructor_ast(&prim_types->arena, intern, "False", NULL);
    NecroASTNode* bool_constructor_list = necro_create_ast_list(&prim_types->arena, true_constructor, necro_create_ast_list(&prim_types->arena, false_constructor, NULL));
    NecroPrimDef* bool_data_def         = necro_prim_def_data(prim_types, intern, &prim_types->bool_type, necro_create_data_declaration_ast(&prim_types->arena, intern, bool_s_type, bool_constructor_list));
    NecroASTNode* bool_type_conid       = necro_create_conid_ast(&prim_types->arena, intern, "Bool", NECRO_CON_TYPE_VAR);
    NecroASTNode* bool_comp_sig_ast     = necro_create_fun_type_sig_ast(&prim_types->arena, intern, "and", NULL, necro_create_fun_ast(&prim_types->arena, bool_type_conid, necro_create_fun_ast(&prim_types->arena, bool_type_conid, bool_type_conid)), NECRO_VAR_DECLARATION, NECRO_SIG_DECLARATION);
    NecroPrimDef* and_fun_def           = necro_prim_def_fun(prim_types, intern, &prim_types->bin_op_types.and_type, bool_comp_sig_ast);
    NecroPrimDef* ord_fun_def           = necro_prim_def_fun(prim_types, intern, &prim_types->bin_op_types.or_type, bool_comp_sig_ast);

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

    // Char
    NecroASTNode* char_s_type   = necro_create_simple_type_ast(&prim_types->arena, intern, "Char", NULL);
    NecroPrimDef* char_data_def = necro_prim_def_data(prim_types, intern, &prim_types->char_type, necro_create_data_declaration_ast(&prim_types->arena, intern, char_s_type, NULL));

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
    necro_create_prim_num_instances(prim_types, intern, "Int", false);
    necro_create_prim_num_instances(prim_types, intern, "Float", true);
    necro_create_prim_num_instances(prim_types, intern, "Audio", true);
    necro_create_prim_num_instances(prim_types, intern, "Rational", true);

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

    // TODO: Finish / Fix
    // // Applicative
    // NecroASTNode* pure_method_sig =
    //     necro_create_fun_type_sig_ast(&prim_types->arena, intern, "pure", NULL, necro_create_fun_ast(&prim_types->arena, a_var, necro_create_type_app_ast(&prim_types->arena, f_var, a_var)), NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
    // NecroASTNode* ap_method_sig  =
    //     necro_create_fun_type_sig_ast(&prim_types->arena, intern, "ap", NULL,
    //         necro_create_fun_ast(&prim_types->arena,
    //             necro_create_type_app_ast(&prim_types->arena, f_var, necro_create_fun_ast(&prim_types->arena, a_var, b_var)),
    //             necro_create_fun_ast(&prim_types->arena,
    //                 necro_create_type_app_ast(&prim_types->arena, f_var, a_var),
    //                 necro_create_type_app_ast(&prim_types->arena, f_var, b_var))),
    //             NECRO_VAR_CLASS_SIG, NECRO_SIG_TYPE_CLASS);
    // NecroASTNode* applicative_method_list = necro_create_declaration_list(&prim_types->arena, pure_method_sig, necro_create_declaration_list(&prim_types->arena, ap_method_sig, NULL));
    // NecroASTNode* applicative_class_ast   = necro_create_type_class_ast(&prim_types->arena, intern, "Applicative", "f", necro_create_context(&prim_types->arena, intern, "Functor", "f", NULL), applicative_method_list);
    // NecroPrimDef* applicative_class_def   = necro_prim_def_class(prim_types, intern, &prim_types->applicative_type_class, applicative_class_ast);

}
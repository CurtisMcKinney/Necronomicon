/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include "prim.h"

//=====================================================
// Symbols
//=====================================================
NecroPrimTypes necro_create_prim_types(NecroIntern* intern)
{

    // BinOp
    NecroBinOpTypes bin_op_types = (NecroBinOpTypes)
    {
        .add_type                = (NecroCon) { .symbol = necro_intern_string(intern, "+"),   .id = { 0 } },
        .sub_type                = (NecroCon) { .symbol = necro_intern_string(intern, "-"),   .id = { 0 } },
        .mul_type                = (NecroCon) { .symbol = necro_intern_string(intern, "*"),   .id = { 0 } },
        .div_type                = (NecroCon) { .symbol = necro_intern_string(intern, "/"),   .id = { 0 } },
        .mod_type                = (NecroCon) { .symbol = necro_intern_string(intern, "%"),   .id = { 0 } },
        .eq_type                 = (NecroCon) { .symbol = necro_intern_string(intern, "=="),  .id = { 0 } },
        .not_eq_type             = (NecroCon) { .symbol = necro_intern_string(intern, "/="),  .id = { 0 } },
        .gt_type                 = (NecroCon) { .symbol = necro_intern_string(intern, ">"),   .id = { 0 } },
        .lt_type                 = (NecroCon) { .symbol = necro_intern_string(intern, "<"),   .id = { 0 } },
        .gte_type                = (NecroCon) { .symbol = necro_intern_string(intern, "=>"),  .id = { 0 } },
        .lte_type                = (NecroCon) { .symbol = necro_intern_string(intern, "<="),  .id = { 0 } },
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

    // Tuple
    NecroTupleTypes tuple_types = (NecroTupleTypes)
    {
        .two   = (NecroCon) { .symbol = necro_intern_string(intern, "(,)"), .id = { 0 } },
        .three = (NecroCon) { .symbol = necro_intern_string(intern, "(,,)"), .id = { 0 } },
        .four  = (NecroCon) { .symbol = necro_intern_string(intern, "(,,,)"), .id = { 0 } },
        .five  = (NecroCon) { .symbol = necro_intern_string(intern, "(,,,,)"), .id = { 0 } },
        .six   = (NecroCon) { .symbol = necro_intern_string(intern, "(,,,,,)"), .id = { 0 } },
        .seven = (NecroCon) { .symbol = necro_intern_string(intern, "(,,,,,,)"), .id = { 0 } },
        .eight = (NecroCon) { .symbol = necro_intern_string(intern, "(,,,,,,,)"), .id = { 0 } },
        .nine  = (NecroCon) { .symbol = necro_intern_string(intern, "(,,,,,,,,)"), .id = { 0 } },
        .ten   = (NecroCon) { .symbol = necro_intern_string(intern, "(,,,,,,,,,)"), .id = { 0 } },
    };

    // PrimSymbols
    return (NecroPrimTypes)
    {
        .bin_op_types = bin_op_types,
        .tuple_types  = tuple_types,
        .io_type      = (NecroCon) { .symbol = necro_intern_string(intern, "IO"),    .id = { 0 } },
        .list_type    = (NecroCon) { .symbol = necro_intern_string(intern, "[]"),    .id = { 0 } },
        .unit_type    = (NecroCon) { .symbol = necro_intern_string(intern, "()"),    .id = { 0 } },
        .int_type     = (NecroCon) { .symbol = necro_intern_string(intern, "Int"),   .id = { 0 } },
        .float_type   = (NecroCon) { .symbol = necro_intern_string(intern, "Float"), .id = { 0 } },
        .audio_type   = (NecroCon) { .symbol = necro_intern_string(intern, "Audio"), .id = { 0 } },
        .char_type    = (NecroCon) { .symbol = necro_intern_string(intern, "Char"),  .id = { 0 } },
        .bool_type    = (NecroCon) { .symbol = necro_intern_string(intern, "Bool"),  .id = { 0 } },
    };
}

//=====================================================
// SymbolInfo and IDs
//=====================================================
inline void necro_add_bin_op_prim_symbol_info(NecroScopedSymTable* scoped_symtable, NecroCon* conid)
{
    NecroSymbolInfo symbol_info = (NecroSymbolInfo)
    {
        .name       = conid->symbol,
        .data_size  = 2,
        .source_loc = (NecroSourceLoc) {0, 0, 0},
        .scope      = scoped_symtable->top_scope,
    };
    conid->id = necro_scoped_symtable_new_symbol_info(scoped_symtable, scoped_symtable->top_scope, symbol_info);
}

inline void necro_add_tuple_symbol_info(NecroScopedSymTable* scoped_symtable, NecroCon* conid, size_t data_size)
{
    NecroSymbolInfo symbol_info = (NecroSymbolInfo)
    {
        .name       = conid->symbol,
        .data_size  = data_size,
        .source_loc = (NecroSourceLoc) {0, 0, 0},
        .scope      = scoped_symtable->top_scope,
    };
    conid->id = necro_scoped_symtable_new_symbol_info(scoped_symtable, scoped_symtable->top_scope, symbol_info);
}

inline void necro_add_type_symbol_info(NecroScopedSymTable* scoped_symtable, NecroCon* conid, size_t data_size)
{
    NecroSymbolInfo symbol_info = (NecroSymbolInfo)
    {
        .name       = conid->symbol,
        .data_size  = data_size,
        .source_loc = (NecroSourceLoc) {0, 0, 0},
        .scope      = scoped_symtable->top_type_scope,
    };
    conid->id = necro_scoped_symtable_new_symbol_info(scoped_symtable, scoped_symtable->top_type_scope, symbol_info);
}

void necro_add_prim_type_symbol_info(NecroPrimTypes* prim_types, NecroScopedSymTable* scoped_symtable)
{
    // Bin Ops
    necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.add_type);
    necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.sub_type);
    necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.mul_type);
    necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.div_type);
    necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.mod_type);
    necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.eq_type);
    necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.not_eq_type);
    necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.gt_type);
    necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.lt_type);
    necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.gte_type);
    necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.lte_type);
    necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.and_type);
    necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.or_type);
    necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.double_colon_type);
    necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.left_shift_type);
    necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.right_shift_type);
    necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.pipe_type);
    necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.forward_pipe_type);
    necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.back_pipe_type);
    necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.dot_type);
    necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.bind_right_type);
    necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.bind_left_type);
    necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.double_exclamation_type);
    necro_add_bin_op_prim_symbol_info(scoped_symtable, &prim_types->bin_op_types.append_type);

    // Tuples
    necro_add_tuple_symbol_info(scoped_symtable, &prim_types->tuple_types.two,   2);
    necro_add_tuple_symbol_info(scoped_symtable, &prim_types->tuple_types.three, 3);
    necro_add_tuple_symbol_info(scoped_symtable, &prim_types->tuple_types.four,  4);
    necro_add_tuple_symbol_info(scoped_symtable, &prim_types->tuple_types.five,  5);
    necro_add_tuple_symbol_info(scoped_symtable, &prim_types->tuple_types.six,   6);
    necro_add_tuple_symbol_info(scoped_symtable, &prim_types->tuple_types.seven, 7);
    necro_add_tuple_symbol_info(scoped_symtable, &prim_types->tuple_types.eight, 8);
    necro_add_tuple_symbol_info(scoped_symtable, &prim_types->tuple_types.nine,  9);
    necro_add_tuple_symbol_info(scoped_symtable, &prim_types->tuple_types.ten,   10);

    // Types
    necro_add_type_symbol_info(scoped_symtable, &prim_types->io_type,    1);
    necro_add_type_symbol_info(scoped_symtable, &prim_types->list_type,  1);
    necro_add_type_symbol_info(scoped_symtable, &prim_types->unit_type,  0);
    necro_add_type_symbol_info(scoped_symtable, &prim_types->int_type,   1);
    necro_add_type_symbol_info(scoped_symtable, &prim_types->float_type, 1);
    necro_add_type_symbol_info(scoped_symtable, &prim_types->audio_type, 0);
    necro_add_type_symbol_info(scoped_symtable, &prim_types->char_type,  1);
    necro_add_type_symbol_info(scoped_symtable, &prim_types->bool_type,  1);
}

//=====================================================
// Type Sigs
//=====================================================
void necro_add_bin_op_prim_type_sig(NecroInfer* infer, NecroCon conid, NecroType* left_type, NecroType* right_type, NecroType* result_type)
{
    assert(conid.id.id < infer->symtable->count);
    necro_symtable_get(infer->symtable, conid.id)->type               = necro_create_type_fun(infer, left_type, necro_create_type_fun(infer, right_type, result_type));
    necro_symtable_get(infer->symtable, conid.id)->type->pre_supplied = true;
}

void necro_add_prim_type_sigs(NecroPrimTypes prim_types, NecroInfer* infer)
{
    NecroType* int_type   = necro_create_type_con(infer, prim_types.int_type, NULL, 0);
    NecroType* bool_type  = necro_create_type_con(infer, prim_types.bool_type, NULL, 0);

    necro_symtable_get(infer->symtable, prim_types.int_type.id)->type                 = int_type;
    necro_symtable_get(infer->symtable, prim_types.int_type.id)->type->pre_supplied   = true;
    necro_symtable_get(infer->symtable, prim_types.bool_type.id)->type                = bool_type;
    necro_symtable_get(infer->symtable, prim_types.bool_type.id)->type->pre_supplied  = true;
    necro_symtable_get(infer->symtable, prim_types.float_type.id)->type               = necro_create_type_con(infer, prim_types.float_type, NULL, 0);
    necro_symtable_get(infer->symtable, prim_types.float_type.id)->type->pre_supplied = true;
    necro_symtable_get(infer->symtable, prim_types.unit_type.id)->type                = necro_create_type_con(infer, prim_types.unit_type, NULL, 0);
    necro_symtable_get(infer->symtable, prim_types.unit_type.id)->type->pre_supplied  = true;
    necro_symtable_get(infer->symtable, prim_types.char_type.id)->type                = necro_create_type_con(infer, prim_types.char_type, NULL, 0);
    necro_symtable_get(infer->symtable, prim_types.char_type.id)->type->pre_supplied  = true;
    necro_symtable_get(infer->symtable, prim_types.audio_type.id)->type               = necro_create_type_con(infer, prim_types.audio_type, NULL, 0);
    necro_symtable_get(infer->symtable, prim_types.audio_type.id)->type->pre_supplied = true;
    necro_symtable_get(infer->symtable, prim_types.list_type.id)->type                = necro_create_type_con(infer, prim_types.list_type, NULL, 1);
    necro_symtable_get(infer->symtable, prim_types.list_type.id)->type->pre_supplied  = true;
    necro_symtable_get(infer->symtable, prim_types.io_type.id)->type                  = necro_create_type_con(infer, prim_types.io_type, NULL, 1);
    necro_symtable_get(infer->symtable, prim_types.io_type.id)->type->pre_supplied    = true;

    necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.add_type, int_type, int_type, int_type);
    necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.sub_type, int_type, int_type, int_type);
    necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.mul_type, int_type, int_type, int_type);
    necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.div_type, int_type, int_type, int_type);
    necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.mod_type, int_type, int_type, int_type);

    necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.eq_type, int_type, int_type, bool_type);
    necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.not_eq_type, int_type, int_type, bool_type);
    necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.gt_type, int_type, int_type, bool_type);
    necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.lt_type, int_type, int_type, bool_type);
    necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.gte_type, int_type, int_type, bool_type);
    necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.lte_type, int_type, int_type, bool_type);
    necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.and_type, bool_type, bool_type, bool_type);
    necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.or_type, bool_type, bool_type, bool_type);

    // TODO: Finish Tuple types

    // TODO: Finish other operators
    // necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.bind_right_type);
    // necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.bind_left_type);
    // necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.double_exclamation_type);
    // necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.append_type);
    // necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.dot_type);
    // necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.back_pipe_type);
    // necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.forward_pipe_type);
    // necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.pipe_type);
    // necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.left_shift_type);
    // necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.right_shift_type);
    // necro_add_bin_op_prim_type_sig(infer, prim_types.bin_op_types.double_colon_type);
}
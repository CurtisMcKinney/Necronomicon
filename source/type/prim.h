/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef TYPE_PRIM_H
#define TYPE_PRIM_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "utility.h"
#include "utility/intern.h"
#include "type.h"
#include "type_class.h"
#include "symtable.h"

typedef NecroAST_Node_Reified NecroASTNode;
//=====================================================
// NecroPrimDef
//=====================================================
struct NecroPrimDef;
typedef enum
{
    NECRO_PRIM_DEF_TYPE,
    NECRO_PRIM_DEF_CON,
    NECRO_PRIM_DEF_VAL,
    NECRO_PRIM_DEF_FUN,
    NECRO_PRIM_DEF_CLASS,
    NECRO_PRIM_DEF_INSTANCE,
} NECRO_PRIM_DEF;

typedef struct
{
    NecroType*       type;
    NecroType*       fully_applied_type;
    NecroASTNode*    ast;
    NecroSymbolInfo  symbol_info;
} NecroPrimDefType;

typedef struct
{
    struct NecroPrimDef* type_def;
    NecroType*           type;
    NecroASTNode*        ast;
    NecroSymbolInfo      symbol_info;
} NecroPrimDefCon;

typedef struct
{
    NecroType*      type;
    NecroASTNode*   ast;
    NecroSymbolInfo symbol_info;
} NecroPrimDefVal;

typedef struct
{
    NecroType*      type;
    NecroASTNode*   ast;
    NecroSymbolInfo symbol_info;
} NecroPrimDefFun;

typedef struct
{
    NecroTypeClass* type_class;
} NecroPrimDefClass;

typedef struct
{
    NecroTypeClassInstance* instance;
} NecroPrimDefInstance;

typedef struct NecroPrimDef
{
    union
    {
        NecroPrimDefType     type_def;
        NecroPrimDefCon      con_def;
        NecroPrimDefVal      val_def;
        NecroPrimDefClass    class_def;
        NecroPrimDefInstance instance_def;
        NecroPrimDefFun      fun_def;
    };
    NecroCon              name;
    struct NecroPrimDef*  next;
    NECRO_PRIM_DEF        type;
} NecroPrimDef;

NECRO_DECLARE_ARENA_CHAIN_TABLE(NecroPrimDef, PrimDef, prim_def);

//=====================================================
// PrimTypes
//=====================================================
typedef struct
{
    NecroCon two;
    NecroCon three;
    NecroCon four;
    NecroCon five;
    NecroCon six;
    NecroCon seven;
    NecroCon eight;
    NecroCon nine;
    NecroCon ten;
} NecroTupleTypes;

typedef struct
{
    NecroCon add_type;
    NecroCon sub_type;
    NecroCon mul_type;
    NecroCon div_type;
    NecroCon mod_type;
    NecroCon eq_type;
    NecroCon not_eq_type;
    NecroCon gt_type;
    NecroCon lt_type;
    NecroCon gte_type;
    NecroCon lte_type;
    NecroCon and_type;
    NecroCon or_type;
    NecroCon double_colon_type;
    NecroCon left_shift_type;
    NecroCon right_shift_type;
    NecroCon pipe_type;
    NecroCon forward_pipe_type;
    NecroCon back_pipe_type;
    NecroCon dot_type;
    NecroCon bind_right_type;
    NecroCon bind_left_type;
    NecroCon double_exclamation_type;
    NecroCon append_type;
} NecroBinOpTypes;

typedef struct NecroPrimTypes
{
    NecroTupleTypes   tuple_types;
    NecroBinOpTypes   bin_op_types;
    NecroCon          io_type;
    NecroCon          list_type;
    NecroCon          int_type;
    NecroCon          float_type;
    NecroCon          audio_type;
    NecroCon          char_type;
    NecroCon          bool_type;
    // NecroPrimDefTable type_def_table;
    // NecroPrimDefTable val_def_table;
    NecroPrimDef*     defs;
    NecroPrimDef*     def_head;
    NecroPagedArena   arena;
} NecroPrimTypes;

//=====================================================
// API
//=====================================================
NecroPrimTypes necro_create_prim_types(NecroIntern* intern);
void           necro_add_prim_type_symbol_info(NecroPrimTypes* prim_types, NecroScopedSymTable* scoped_symtable);
void           necro_add_prim_type_sigs(NecroPrimTypes* prim_types, NecroInfer* infer);

#endif // TYPE_PRIM_H
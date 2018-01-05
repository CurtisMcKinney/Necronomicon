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
#include "renamer.h"
#include "d_analyzer.h"

typedef NecroAST_Node_Reified NecroASTNode;
//=====================================================
// NecroPrimDef
//=====================================================
struct NecroPrimDef;
typedef enum
{
    NECRO_PRIM_DEF_DATA,
    NECRO_PRIM_DEF_FUN,
    NECRO_PRIM_DEF_BIN_OP,
    NECRO_PRIM_DEF_CLASS,
    NECRO_PRIM_DEF_INSTANCE,
} NECRO_PRIM_DEF;

typedef struct
{
    NecroType*      type_type;
    NecroType*      type_fully_applied_type;
    NecroASTNode*   data_declaration_ast;
} NecroPrimDefData;

typedef struct
{
    NecroType*      type;
    NecroASTNode*   type_sig_ast;
} NecroPrimDefFun;

typedef struct
{
    NecroType*      type;
    NecroASTNode*   type_sig_ast;
    NecroASTNode*   definition_ast;
} NecroPrimDefBinOp;

typedef struct
{
    NecroTypeClass* type_class;
    NecroASTNode*   type_class_ast;
} NecroPrimDefClass;

typedef struct
{
    NecroTypeClassInstance* instance;
    NecroASTNode*           instance_ast;
} NecroPrimDefInstance;

typedef struct NecroPrimDef
{
    union
    {
        NecroPrimDefClass    class_def;
        NecroPrimDefInstance instance_def;
        NecroPrimDefFun      fun_def;
        NecroPrimDefData     data_def;
        NecroPrimDefBinOp    bin_op_def;
    };
    NecroCon              name;
    NecroCon*             global_name;
    struct NecroPrimDef*  next;
    NECRO_PRIM_DEF        type;
} NecroPrimDef;

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

// typedef struct
// {
//     NecroType* add_type;
//     NecroType* sub_type;
//     NecroType* mul_type;
//     NecroType* div_type;
//     NecroType* eq_type;
//     NecroType* not_eq_type;
//     NecroType* gt_type;
//     NecroType* lt_type;
//     NecroType* gte_type;
//     NecroType* lte_type;
//     NecroType* cons_type;
//     NecroCon mod_type;
//     NecroCon and_type;
//     NecroCon or_type;
//     NecroCon double_colon_type;
//     NecroCon left_shift_type;
//     NecroCon right_shift_type;
//     NecroCon pipe_type;
//     NecroCon forward_pipe_type;
//     NecroCon back_pipe_type;
//     NecroCon dot_type;
//     NecroCon bind_right_type;
//     NecroCon bind_left_type;
//     NecroCon double_exclamation_type;
//     NecroCon append_type;
// } NecroBinOpTypes;

typedef struct NecroPrimTypes
{
    NecroTupleTypes   tuple_types;
    // NecroBinOpTypes   bin_op_types;
    NecroCon          io_type;
    NecroCon          unit_type;
    NecroCon          unit_con;
    NecroCon          list_type;
    NecroCon          int_type;
    NecroCon          float_type;
    NecroCon          audio_type;
    NecroCon          rational_type;
    NecroCon          char_type;
    NecroCon          bool_type;
    NecroCon          num_type_class;
    NecroCon          fractional_type_class;
    NecroCon          eq_type_class;
    NecroCon          ord_type_class;
    NecroCon          functor_type_class;
    NecroCon          applicative_type_class;
    NecroCon          monad_type_class;
    NecroPrimDef*     defs;
    NecroPrimDef*     def_head;
    NecroPagedArena   arena;
} NecroPrimTypes;

//=====================================================
// API
//=====================================================
NecroPrimTypes necro_create_prim_types(NecroIntern* intern);

NECRO_RETURN_CODE necro_prim_build_scope(NecroPrimTypes* prim_types, NecroScopedSymTable* scoped_symtable);
NECRO_RETURN_CODE necro_prim_rename(NecroPrimTypes* prim_types, NecroRenamer* renamer);
// NECRO_RETURN_CODE necro_prim_d_analyze(NecroPrimTypes* prim_types, NecroDependencyAnalyzer* d_analyzer);
NECRO_RETURN_CODE necro_prim_infer(NecroPrimTypes* prim_types, NecroDependencyAnalyzer* d_analyzer, NecroInfer* infer);
void              necro_init_prim_defs(NecroPrimTypes* prim_types, NecroIntern* intern);

#endif // TYPE_PRIM_H
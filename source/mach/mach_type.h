/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_MACH_TYPE_H
#define NECRO_MACH_TYPE_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "core/core.h"
#include "type.h"

struct NecroMachProgram;
struct NecroMachAstSymbol;

typedef enum
{
    NECRO_LANG_CALL,
    NECRO_C_CALL,
} NECRO_MACH_CALL_TYPE;

typedef enum
{
    NECRO_MACH_TYPE_UINT1,
    NECRO_MACH_TYPE_UINT8,
    NECRO_MACH_TYPE_UINT16,
    NECRO_MACH_TYPE_UINT32,
    NECRO_MACH_TYPE_UINT64,
    NECRO_MACH_TYPE_INT32,
    NECRO_MACH_TYPE_INT64,
    NECRO_MACH_TYPE_F32,
    NECRO_MACH_TYPE_F64,
    NECRO_MACH_TYPE_CHAR,
    NECRO_MACH_TYPE_STRUCT,
    NECRO_MACH_TYPE_FN,
    NECRO_MACH_TYPE_PTR,
    NECRO_MACH_TYPE_VOID,
} NECRO_MACH_TYPE_TYPE;

typedef struct NecroMachStructType
{
    struct NecroMachAstSymbol* symbol;
    struct NecroMachType**     members;
    size_t                     num_members;
} NecroMachStructType;

typedef struct NecroMachPtrType
{
    struct NecroMachType* element_type;
} NecroMachPtrType;

typedef struct NecroMachFnType
{
    struct NecroMachType*  return_type;
    struct NecroMachType** parameters;
    size_t                 num_parameters;
} NecroMachFnType;

typedef struct NecroMachType
{
    union
    {
        NecroMachStructType struct_type;
        NecroMachPtrType    ptr_type;
        NecroMachFnType     fn_type;
    };
    NECRO_MACH_TYPE_TYPE type;
} NecroMachType;

NecroMachType* necro_mach_type_create_word_sized_uint(struct NecroMachProgram* program);
NecroMachType* necro_mach_type_create_word_sized_int(struct NecroMachProgram* program);
NecroMachType* necro_mach_type_create_word_sized_float(struct NecroMachProgram* program);
NecroMachType* necro_mach_type_create_uint1(NecroPagedArena* arena);
NecroMachType* necro_mach_type_create_uint8(NecroPagedArena* arena);
NecroMachType* necro_mach_type_create_uint16(NecroPagedArena* arena);
NecroMachType* necro_mach_type_create_uint32(NecroPagedArena* arena);
NecroMachType* necro_mach_type_create_uint64(NecroPagedArena* arena);
NecroMachType* necro_mach_type_create_int32(NecroPagedArena* arena);
NecroMachType* necro_mach_type_create_int64(NecroPagedArena* arena);
NecroMachType* necro_mach_type_create_f32(NecroPagedArena* arena);
NecroMachType* necro_mach_type_create_f64(NecroPagedArena* arena);
NecroMachType* necro_mach_type_create_char(NecroPagedArena* arena);
NecroMachType* necro_mach_type_create_struct(NecroPagedArena* arena, struct NecroMachAstSymbol* symbol, NecroMachType** a_members, size_t num_members);
NecroMachType* necro_mach_type_create_fn(NecroPagedArena* arena, NecroMachType* return_type, NecroMachType** a_parameters, size_t num_parameters);
NecroMachType* necro_mach_type_create_ptr(NecroPagedArena* arena, NecroMachType* element_type);
NecroMachType* necro_mach_type_create_void(NecroPagedArena* arena);
void           necro_mach_type_check(struct NecroMachProgram* program, NecroMachType* type1, NecroMachType* type2);
bool           necro_mach_type_is_unboxed(struct NecroMachProgram* program, NecroMachType* type);
bool           necro_mach_type_is_word_uint(struct NecroMachProgram* program, NecroMachType* type);
NecroMachType* necro_mach_type_make_ptr_if_boxed(struct NecroMachProgram* program, NecroMachType* type);
void           necro_mach_type_print(NecroMachType* type);
void           necro_mach_type_print_go(NecroMachType* type, bool is_recursive);
NecroMachType* necro_mach_type_from_necro_type(struct NecroMachProgram* program, NecroType* type);
// NecroMachType* necro_core_ast_to_machine_type(struct NecroMachProgram* program, NecroCoreAST_Expression* core_ast);
// NecroType*     necro_core_ast_to_necro_type(struct NecroMachProgram* program, NecroCoreAST_Expression* ast);
// NecroMachType* necro_core_pattern_type_to_machine_type(struct NecroMachProgram* program, NecroCoreAST_Expression* ast);

#endif // NECRO_MACH_TYPE_H
/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_MACHINE_TYPE_H
#define NECRO_MACHINE_TYPE_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "core/core.h"
#include "type.h"

struct NecroMachineProgram;

typedef enum
{
    NECRO_STATE_CONSTANT  = 0,
    NECRO_STATE_POINTWISE = 1,
    NECRO_STATE_STATEFUL  = 2,
} NECRO_STATE_TYPE;

typedef enum
{
    NECRO_MACHINE_TYPE_UINT1,
    NECRO_MACHINE_TYPE_UINT8,
    NECRO_MACHINE_TYPE_UINT16,
    NECRO_MACHINE_TYPE_UINT32,
    NECRO_MACHINE_TYPE_UINT64,
    NECRO_MACHINE_TYPE_INT32,
    NECRO_MACHINE_TYPE_INT64,
    NECRO_MACHINE_TYPE_F32,
    NECRO_MACHINE_TYPE_F64,
    NECRO_MACHINE_TYPE_CHAR,
    NECRO_MACHINE_TYPE_STRUCT,
    NECRO_MACHINE_TYPE_FN,
    NECRO_MACHINE_TYPE_PTR,
    NECRO_MACHINE_TYPE_VOID,
} NECRO_MACHINE_TYPE_TYPE;

typedef struct NecroMachineType
{
    union
    {
        struct NecroMachineStructType
        {
            NecroVar               name;
            struct NecroMachineType** members;
            size_t                 num_members;
        } struct_type;
        struct NecroMachinePtrType
        {
            struct NecroMachineType* element_type; // poly_type sentinel machine flags that it is a poly type
        } ptr_type;
        struct NecroMachineFnType
        {
            // NecroVar               name;
            struct NecroMachineType*  return_type;
            struct NecroMachineType** parameters;
            size_t                 num_parameters;
        } fn_type;
    };
    NECRO_MACHINE_TYPE_TYPE type;
} NecroMachineType;

NecroMachineType* necro_create_word_sized_uint_type(struct NecroMachineProgram* program);
NecroMachineType* necro_create_word_sized_int_type(struct NecroMachineProgram* program);
NecroMachineType* necro_create_word_sized_float_type(struct NecroMachineProgram* program);
NecroMachineType* necro_create_machine_uint1_type(NecroPagedArena* arena);
NecroMachineType* necro_create_machine_uint8_type(NecroPagedArena* arena);
NecroMachineType* necro_create_machine_uint16_type(NecroPagedArena* arena);
NecroMachineType* necro_create_machine_uint32_type(NecroPagedArena* arena);
NecroMachineType* necro_create_machine_uint64_type(NecroPagedArena* arena);
NecroMachineType* necro_create_machine_int32_type(NecroPagedArena* arena);
NecroMachineType* necro_create_machine_int64_type(NecroPagedArena* arena);
NecroMachineType* necro_create_machine_f32_type(NecroPagedArena* arena);
NecroMachineType* necro_create_machine_f64_type(NecroPagedArena* arena);
NecroMachineType* necro_create_machine_char_type(NecroPagedArena* arena);
NecroMachineType* necro_create_machine_struct_type(NecroPagedArena* arena, NecroVar name, NecroMachineType** a_members, size_t num_members);
NecroMachineType* necro_create_machine_fn_type(NecroPagedArena* arena, NecroMachineType* return_type, NecroMachineType** a_parameters, size_t num_parameters);
NecroMachineType* necro_create_machine_ptr_type(NecroPagedArena* arena, NecroMachineType* element_type);
NecroMachineType* necro_create_machine_void_type(NecroPagedArena* arena);
void              necro_type_check(struct NecroMachineProgram* program, NecroMachineType* type1, NecroMachineType* type2);
void              necro_machine_print_machine_type(NecroIntern* intern, NecroMachineType* type);
void              necro_machine_print_machine_type_go(NecroIntern* intern, NecroMachineType* type, bool is_recursive);
NecroMachineType* necro_core_ast_to_machine_type(struct NecroMachineProgram* program, NecroCoreAST_Expression* core_ast);
NecroType*        necro_core_ast_to_necro_type(struct NecroMachineProgram* program, NecroCoreAST_Expression* ast);
NecroMachineType* necro_type_to_machine_type(struct NecroMachineProgram* program, NecroType* type);
bool              is_poly_ptr(struct NecroMachineProgram* program, NecroMachineType* type);
NecroMachineType* necro_core_pattern_type_to_machine_type(struct NecroMachineProgram* program, NecroCoreAST_Expression* ast);
bool              necro_is_unboxed_type(struct NecroMachineProgram* program, NecroMachineType* type);
bool              necro_is_word_uint_type(struct NecroMachineProgram* program, NecroMachineType* type);
NecroMachineType* necro_make_ptr_if_boxed(struct NecroMachineProgram* program, NecroMachineType* type);

#endif // NECRO_MACHINE_TYPE_H
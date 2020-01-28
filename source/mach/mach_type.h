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

#include "type.h"

struct NecroMachProgram;
struct NecroMachAstSymbol;
struct NecroMachAst;

typedef enum
{
    NECRO_MACH_CALL_LANG,
    NECRO_MACH_CALL_C,
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
    NECRO_MACH_TYPE_ARRAY,
    NECRO_MACH_TYPE_VOID,
} NECRO_MACH_TYPE_TYPE;

typedef struct NecroMachStructType
{
    struct NecroMachAstSymbol* symbol;
    struct NecroMachType**     members;
    size_t                     num_members;
    struct NecroMachAstSymbol* sum_type_symbol;
} NecroMachStructType;

typedef struct NecroMachPtrType
{
    struct NecroMachType* element_type;
} NecroMachPtrType;

typedef struct NecroMachArrayType
{
    struct NecroMachType* element_type;
    size_t                element_count;
} NecroMachArrayType;

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
        NecroMachArrayType  array_type;
    };
    NECRO_MACH_TYPE_TYPE type;
} NecroMachType;

typedef struct NecroMachTypeCacheBucket
{
    size_t              hash;
    NecroType*          necro_type;
    NecroMachType*      mach_type;
} NecroMachTypeCacheBucket;

typedef struct NecroMachTypeCache
{
    NecroMachType*            uint1_type;
    NecroMachType*            uint8_type;
    NecroMachType*            uint16_type;
    NecroMachType*            uint32_type;
    NecroMachType*            uint64_type;
    NecroMachType*            int32_type;
    NecroMachType*            int64_type;
    NecroMachType*            f32_type;
    NecroMachType*            f64_type;
    NecroMachType*            char_type;
    NecroMachType*            void_type;
    NecroMachType*            word_uint_type;
    NecroMachType*            word_int_type;
    NecroMachTypeCacheBucket* buckets;
    size_t                    count;
    size_t                    capacity;
} NecroMachTypeCache;

//--------------------
// Create
//--------------------
NecroMachType*     necro_mach_type_create_word_sized_uint(struct NecroMachProgram* program);
NecroMachType*     necro_mach_type_create_word_sized_int(struct NecroMachProgram* program);
NecroMachType*     necro_mach_type_create_uint1(struct NecroMachProgram* program);
NecroMachType*     necro_mach_type_create_uint8(struct NecroMachProgram* program);
NecroMachType*     necro_mach_type_create_uint16(struct NecroMachProgram* program);
NecroMachType*     necro_mach_type_create_uint32(struct NecroMachProgram* program);
NecroMachType*     necro_mach_type_create_uint64(struct NecroMachProgram* program);
NecroMachType*     necro_mach_type_create_int32(struct NecroMachProgram* program);
NecroMachType*     necro_mach_type_create_int64(struct NecroMachProgram* program);
NecroMachType*     necro_mach_type_create_f32(struct NecroMachProgram* program);
NecroMachType*     necro_mach_type_create_f64(struct NecroMachProgram* program);
NecroMachType*     necro_mach_type_create_char(struct NecroMachProgram* program);
NecroMachType*     necro_mach_type_create_void(struct NecroMachProgram* program);
NecroMachType*     necro_mach_type_create_struct(NecroPagedArena* arena, struct NecroMachAstSymbol* symbol, NecroMachType** a_members, size_t num_members);
NecroMachType*     necro_mach_type_create_struct_with_sum_type(NecroPagedArena* arena, struct NecroMachAstSymbol* symbol, NecroMachType** a_members, size_t num_members, struct NecroMachAstSymbol* sum_type_symbol);
NecroMachType*     necro_mach_type_create_fn(NecroPagedArena* arena, NecroMachType* return_type, NecroMachType** a_parameters, size_t num_parameters);
NecroMachType*     necro_mach_type_create_ptr(NecroPagedArena* arena, NecroMachType* element_type);
NecroMachType*     necro_mach_type_create_array(NecroPagedArena* arena, NecroMachType* element_type, size_t element_count);
NecroMachTypeCache necro_mach_type_cache_empty();
NecroMachTypeCache necro_mach_type_cache_create(struct NecroMachProgram* program);
void               necro_mach_type_cache_destroy(NecroMachTypeCache* cache);

//--------------------
// Type Check
//--------------------
bool necro_mach_type_is_eq(NecroMachType* type1, NecroMachType* type2);
void necro_mach_type_check(struct NecroMachProgram* program, NecroMachType* type1, NecroMachType* type2);
void necro_mach_ast_type_check(struct NecroMachProgram* program, struct NecroMachAst* ast);
void necro_mach_type_check_is_int_type(NecroMachType* type1);
void necro_mach_type_check_is_uint_type(NecroMachType* type1);
void necro_mach_type_check_is_float_type(NecroMachType* type1);
void necro_mach_program_verify(struct NecroMachProgram* program);

//--------------------
// Utility
//--------------------
bool           necro_mach_type_is_unboxed(struct NecroMachProgram* program, NecroMachType* type);
bool           necro_mach_type_is_unboxed_or_ptr(struct NecroMachProgram* program, NecroMachType* type);
bool           necro_mach_type_is_word_uint(struct NecroMachProgram* program, NecroMachType* type);
NecroMachType* necro_mach_type_make_ptr_if_boxed(struct NecroMachProgram* program, NecroMachType* type);
void           necro_mach_type_print(NecroMachType* type);
void           necro_mach_type_print_go(NecroMachType* type, bool is_recursive);
NecroMachType* necro_mach_type_from_necro_type(struct NecroMachProgram* program, NecroType* type);
bool           necro_mach_type_is_sum_type(NecroMachType* type);
NecroMachType* necro_mach_type_get_sum_type(NecroMachType* type);
size_t         necro_mach_type_calculate_size_in_bytes(struct NecroMachProgram* program, NecroMachType* type);
size_t         necro_mach_word_size_in_bytes(struct NecroMachProgram* program);


#endif // NECRO_MACH_TYPE_H
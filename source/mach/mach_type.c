/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "mach_type.h"
#include "mach_ast.h"
#include "core_ast.h"
#include "mach_transform.h"
#include "kind.h"
#include "runtime.h"
#include <ctype.h>

///////////////////////////////////////////////////////
// MachTypeCache
///////////////////////////////////////////////////////
NecroMachTypeCache necro_mach_type_cache_empty()
{
    return (NecroMachTypeCache)
    {
        .uint1_type      = NULL,
        .uint8_type      = NULL,
        .uint16_type     = NULL,
        .uint32_type     = NULL,
        .uint64_type     = NULL,
        .int32_type      = NULL,
        .int64_type      = NULL,
        .f32_type        = NULL,
        .f64_type        = NULL,
        .char_type       = NULL,
        .void_type       = NULL,
        .word_uint_type  = NULL,
        .word_int_type   = NULL,
        .word_float_type = NULL,
        .buckets         = NULL,
        .count           = 0,
        .capacity        = 0,
    };
}

NecroMachTypeCache necro_mach_type_cache_create(NecroMachProgram* program)
{
    program->type_cache                 = necro_mach_type_cache_empty();
    // Init Types
    program->type_cache.uint1_type      = necro_mach_type_create_uint1(program),
    program->type_cache.uint8_type      = necro_mach_type_create_uint8(program),
    program->type_cache.uint16_type     = necro_mach_type_create_uint16(program),
    program->type_cache.uint32_type     = necro_mach_type_create_uint32(program),
    program->type_cache.uint64_type     = necro_mach_type_create_uint64(program),
    program->type_cache.int32_type      = necro_mach_type_create_int32(program),
    program->type_cache.int64_type      = necro_mach_type_create_int64(program),
    program->type_cache.f32_type        = necro_mach_type_create_f32(program),
    program->type_cache.f64_type        = necro_mach_type_create_f64(program),
    program->type_cache.char_type       = necro_mach_type_create_char(program),
    program->type_cache.void_type       = necro_mach_type_create_void(program),
    program->type_cache.word_uint_type  = necro_mach_type_create_word_sized_uint(program);
    program->type_cache.word_int_type   = necro_mach_type_create_word_sized_int(program);
    program->type_cache.word_float_type = necro_mach_type_create_word_sized_float(program);
    // Init Table
    const size_t initial_capacity       = 1024;
    program->type_cache.buckets         = emalloc(initial_capacity * sizeof(NecroMachTypeCacheBucket));
    program->type_cache.count           = 0;
    program->type_cache.capacity        = initial_capacity;
    for (size_t i = 0; i < program->type_cache.capacity; ++i)
        program->type_cache.buckets[i] = (NecroMachTypeCacheBucket) { .hash = 0, .necro_type = NULL, .mach_type = NULL };
    return program->type_cache;
}

void necro_mach_type_cache_destroy(NecroMachTypeCache* cache)
{
    free(cache->buckets);
    *cache = necro_mach_type_cache_empty();
}

// TODO: Fix cache grow bug !!!!
void _necro_mach_type_cache_grow(NecroMachTypeCache* cache)
{
    size_t                    old_count    = cache->count;
    size_t                    old_capacity = cache->capacity;
    NecroMachTypeCacheBucket* old_buckets  = cache->buckets;
    cache->count                           = 0;
    cache->capacity                        = old_capacity * 2;
    cache->buckets                         = emalloc(cache->capacity * sizeof(NecroMachTypeCacheBucket));
    for (size_t i = 0; i < cache->capacity; ++i)
        cache->buckets[i] = (NecroMachTypeCacheBucket) { .hash = 0, .necro_type = NULL, .mach_type = NULL };
    for (size_t i = 0; i < old_capacity; ++i)
    {
        NecroMachTypeCacheBucket* bucket = old_buckets + i;
        if (bucket->hash == 0 && bucket->mach_type == NULL && bucket->necro_type == NULL)
            continue;
        size_t bucket_index = bucket->hash & (cache->capacity - 1);
        while (true)
        {
            if (cache->buckets[bucket_index].hash == 0 && cache->buckets[bucket_index].mach_type == NULL && cache->buckets[bucket_index].necro_type == NULL)
            {
                cache->buckets[bucket_index] = *bucket;
                cache->count++;
                break;
            }
            bucket_index = (bucket_index + 1) & (cache->capacity - 1);
        }
    }
    assert(cache->count >= old_count);
    // TODO: Look at this...
    // if (cache->count > old_count);
    // {
    //     fprintf(stderr, "type cache grow off, count: %zu, old_count: %zu\n", cache->count, old_count);
    // }
    free(old_buckets);
}

NecroMachType* _necro_mach_type_cache_get(NecroMachProgram* program, NecroType* type);

NecroMachType* _necro_mach_type_from_necro_type_fn(NecroMachProgram* program, NecroType* type)
{
    assert(type != NULL);
    type = necro_type_find(type);
    assert(type->type == NECRO_TYPE_FUN);
    // Count args
    size_t     arg_count = 0;
    size_t     arg_index = 0;
    NecroType* arrows    = necro_type_find(type);
    while (arrows->type == NECRO_TYPE_FUN)
    {
        arg_count++;
        arrows = arrows->fun.type2;
        arrows = necro_type_find(arrows);
    }
    NecroMachType** args = necro_paged_arena_alloc(&program->arena, arg_count * sizeof(NecroMachType*));
    arrows               = necro_type_find(type);
    while (arrows->type == NECRO_TYPE_FUN)
    {
        args[arg_index] = necro_mach_type_make_ptr_if_boxed(program, _necro_mach_type_cache_get(program, arrows->fun.type1));
        arg_index++;
        arrows          = arrows->fun.type2;
        arrows          = necro_type_find(arrows);
    }
    NecroMachType* return_type   = necro_mach_type_make_ptr_if_boxed(program, _necro_mach_type_cache_get(program, arrows));
    NecroMachType* function_type = necro_mach_type_create_fn(&program->arena, return_type, args, arg_count);
    return function_type;
}

NecroMachType* _necro_mach_type_from_necro_type_poly_con(NecroMachProgram* program, NecroType* type)
{
    // Handle Primitively polymorphic types
    if (type->con.con_symbol == program->base->array_type)
    {
        NecroType*     n                  = type->con.args->list.item;
        size_t         element_count      = necro_nat_to_size_t(program->base, n);
        NecroType*     element_necro_type = type->con.args->list.next->list.item;
        NecroMachType* element_mach_type  = necro_mach_type_make_ptr_if_boxed(program, necro_mach_type_from_necro_type(program, element_necro_type));
        return necro_mach_type_create_array(&program->arena, element_mach_type, element_count);
    }
    else if (type->con.con_symbol == program->base->ptr_type)
    {
        NecroType*     element_necro_type = type->con.args->list.item;
        NecroMachType* element_mach_type  = necro_mach_type_make_ptr_if_boxed(program, necro_mach_type_from_necro_type(program, element_necro_type));
        return necro_mach_type_create_ptr(&program->arena, element_mach_type);
    }
    assert(false);
    return NULL;
}

NecroMachType* _necro_mach_type_from_necro_type(NecroMachProgram* program, NecroType* type)
{
    assert(type != NULL);
    type = necro_type_find(type);
    switch (type->type)
    {
    case NECRO_TYPE_FUN:
        return _necro_mach_type_from_necro_type_fn(program, type);
    case NECRO_TYPE_CON:
        assert(type->con.con_symbol != NULL);
        if (type->con.args == NULL)
        {
            // Monotype
            assert(type->con.con_symbol->core_ast_symbol->mach_symbol != NULL);
            assert(type->con.con_symbol->core_ast_symbol->mach_symbol->mach_type != NULL);
            return type->con.con_symbol->core_ast_symbol->mach_symbol->mach_type;
        }
        else
        {
            // Polytype
            return _necro_mach_type_from_necro_type_poly_con(program, type);
        }
    default:
        assert(false);
        return NULL;
    }
}

/*
    Notes:
        * Should always be monomorphic
        * Constructors should always be completely applied.
*/
NecroMachType* _necro_mach_type_cache_get(NecroMachProgram* program, NecroType* type)
{
    NecroMachTypeCache* cache = &program->type_cache;
    assert(type != NULL);
    // Grow
    if ((cache->count * 2) >= cache->capacity)
        _necro_mach_type_cache_grow(cache);
    // Hash
    type                = necro_type_find(type);
    size_t hash         = necro_type_hash(type);
    size_t bucket_index = hash & (cache->capacity - 1);
    // Find
    while (true)
    {
        NecroMachTypeCacheBucket* bucket = cache->buckets + bucket_index;
        if (bucket->hash == hash && bucket->mach_type != NULL && necro_type_exact_unify(type, bucket->necro_type))
        {
            // Found
            return bucket->mach_type;
        }
        else if (bucket->hash == 0 && bucket->mach_type == NULL && bucket->necro_type == NULL)
        {
            // Create
            bucket->hash       = hash;
            bucket->mach_type  = _necro_mach_type_from_necro_type(program, type);
            assert(bucket->mach_type != NULL);
            bucket->necro_type = type;
            cache->count++;
            return bucket->mach_type;
        }
        bucket_index = (bucket_index + 1) & (cache->capacity - 1);
    }
    assert(false);
    return NULL;
}

NecroMachType* necro_mach_type_from_necro_type(NecroMachProgram* program, NecroType* type)
{
    return _necro_mach_type_cache_get(program, type);
}

///////////////////////////////////////////////////////
// Create
///////////////////////////////////////////////////////
NecroMachType* necro_mach_type_create_word_sized_uint(NecroMachProgram* program)
{
    if (program->type_cache.word_uint_type != NULL)
       return program->type_cache.word_uint_type;
    NecroMachType* type                = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachType));
    type->type                         = (program->word_size == NECRO_WORD_4_BYTES) ? NECRO_MACH_TYPE_UINT32 : NECRO_MACH_TYPE_UINT64;
    program->type_cache.word_uint_type = type;
    return type;
}

NecroMachType* necro_mach_type_create_word_sized_int(NecroMachProgram* program)
{
    if (program->type_cache.word_int_type != NULL)
       return program->type_cache.word_int_type;
    NecroMachType* type               = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachType));
    type->type                        = (program->word_size == NECRO_WORD_4_BYTES) ? NECRO_MACH_TYPE_INT32 : NECRO_MACH_TYPE_INT64;
    program->type_cache.word_int_type = type;
    return type;
}

NecroMachType* necro_mach_type_create_word_sized_float(NecroMachProgram* program)
{
    if (program->type_cache.word_float_type != NULL)
       return program->type_cache.word_float_type;
    NecroMachType* type                 = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachType));
    type->type                          = (program->word_size == NECRO_WORD_4_BYTES) ? NECRO_MACH_TYPE_F32 : NECRO_MACH_TYPE_F64;
    program->type_cache.word_float_type = type;
    return type;
}

NecroMachType* necro_mach_type_create_uint1(NecroMachProgram* program)
{
    if (program->type_cache.uint1_type != NULL)
       return program->type_cache.uint1_type;
    NecroMachType* type            = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachType));
    type->type                     = NECRO_MACH_TYPE_UINT1;
    program->type_cache.uint1_type = type;
    return type;
}

NecroMachType* necro_mach_type_create_uint8(NecroMachProgram* program)
{
    if (program->type_cache.uint8_type != NULL)
       return program->type_cache.uint8_type;
    NecroMachType* type            = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachType));
    type->type                     = NECRO_MACH_TYPE_UINT8;
    program->type_cache.uint8_type = type;
    return type;
}

NecroMachType* necro_mach_type_create_uint16(NecroMachProgram* program)
{
    if (program->type_cache.uint16_type != NULL)
       return program->type_cache.uint16_type;
    NecroMachType* type             = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachType));
    type->type                      = NECRO_MACH_TYPE_UINT16;
    program->type_cache.uint16_type = type;
    return type;
}

NecroMachType* necro_mach_type_create_uint32(NecroMachProgram* program)
{
    if (program->type_cache.uint32_type != NULL)
       return program->type_cache.uint32_type;
    NecroMachType* type             = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachType));
    type->type                      = NECRO_MACH_TYPE_UINT32;
    program->type_cache.uint32_type = type;
    return type;
}

NecroMachType* necro_mach_type_create_uint64(NecroMachProgram* program)
{
    if (program->type_cache.uint64_type != NULL)
       return program->type_cache.uint64_type;
    NecroMachType* type             = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachType));
    type->type                      = NECRO_MACH_TYPE_UINT64;
    program->type_cache.uint64_type = type;
    return type;
}

NecroMachType* necro_mach_type_create_int32(NecroMachProgram* program)
{
    if (program->type_cache.int32_type != NULL)
       return program->type_cache.int32_type;
    NecroMachType* type            = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachType));
    type->type                     = NECRO_MACH_TYPE_INT32;
    program->type_cache.int32_type = type;
    return type;
}

NecroMachType* necro_mach_type_create_int64(NecroMachProgram* program)
{
    if (program->type_cache.int64_type != NULL)
       return program->type_cache.int64_type;
    NecroMachType* type            = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachType));
    type->type                     = NECRO_MACH_TYPE_INT64;
    program->type_cache.int64_type = type;
    return type;
}

NecroMachType* necro_mach_type_create_f32(NecroMachProgram* program)
{
    if (program->type_cache.f32_type != NULL)
       return program->type_cache.f32_type;
    NecroMachType* type          = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachType));
    type->type                   = NECRO_MACH_TYPE_F32;
    program->type_cache.f32_type = type;
    return type;
}

NecroMachType* necro_mach_type_create_f64(NecroMachProgram* program)
{
    if (program->type_cache.f64_type != NULL)
       return program->type_cache.f64_type;
    NecroMachType* type          = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachType));
    type->type                   = NECRO_MACH_TYPE_F64;
    program->type_cache.f64_type = type;
    return type;
}

NecroMachType* necro_mach_type_create_char(NecroMachProgram* program)
{
    if (program->type_cache.char_type != NULL)
       return program->type_cache.char_type;
    NecroMachType* type          = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachType));
    type->type                   = NECRO_MACH_TYPE_CHAR;
    program->type_cache.char_type = type;
    return type;
}

NecroMachType* necro_mach_type_create_void(NecroMachProgram* program)
{
    if (program->type_cache.void_type != NULL)
       return program->type_cache.void_type;
    NecroMachType* type           = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachType));
    type->type                    = NECRO_MACH_TYPE_VOID;
    program->type_cache.void_type = type;
    return type;
}

NecroMachType* necro_mach_type_create_struct_with_sum_type(NecroPagedArena* arena, NecroMachAstSymbol* symbol, NecroMachType** a_members, size_t num_members, NecroMachAstSymbol* sum_type_symbol)
{
    NecroMachType* type               = necro_paged_arena_alloc(arena, sizeof(NecroMachType));
    type->type                        = NECRO_MACH_TYPE_STRUCT;
    type->struct_type.symbol          = symbol;
    type->struct_type.sum_type_symbol = sum_type_symbol;
    NecroMachType** members           = necro_paged_arena_alloc(arena, num_members * sizeof(NecroMachType*));
    memcpy(members, a_members, num_members * sizeof(NecroMachType*)); // This isn't a deep copy...is that ok?
    type->struct_type.members         = members;
    type->struct_type.num_members     = num_members;
    return type;
}

NecroMachType* necro_mach_type_create_struct(NecroPagedArena* arena, NecroMachAstSymbol* symbol, NecroMachType** a_members, size_t num_members)
{
    return necro_mach_type_create_struct_with_sum_type(arena, symbol, a_members, num_members, NULL);
}

NecroMachType* necro_mach_type_create_fn(NecroPagedArena* arena, NecroMachType* return_type, NecroMachType** a_parameters, size_t num_parameters)
{
    NecroMachType* type          = necro_paged_arena_alloc(arena, sizeof(NecroMachType));
    type->type                   = NECRO_MACH_TYPE_FN;
    type->fn_type.return_type    = return_type;
    NecroMachType** parameters   = necro_paged_arena_alloc(arena, num_parameters * sizeof(NecroMachType*));
    memcpy(parameters, a_parameters, num_parameters * sizeof(NecroMachType*));
    type->fn_type.parameters     = parameters;
    type->fn_type.num_parameters = num_parameters;
    return type;
}

NecroMachType* necro_mach_type_create_ptr(NecroPagedArena* arena, NecroMachType* element_type)
{
    assert(element_type != NULL);
    NecroMachType* type         = necro_paged_arena_alloc(arena, sizeof(NecroMachType));
    type->type                  = NECRO_MACH_TYPE_PTR;
    type->ptr_type.element_type = element_type;
    return type;
}

NecroMachType* necro_mach_type_create_array(NecroPagedArena* arena, NecroMachType* element_type, size_t element_count)
{
    assert(element_type != NULL);
    assert(element_count > 0);
    NecroMachType* type            = necro_paged_arena_alloc(arena, sizeof(NecroMachType));
    type->type                     = NECRO_MACH_TYPE_ARRAY;
    type->array_type.element_type  = element_type;
    type->array_type.element_count = element_count;
    return type;
}

///////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////
bool necro_mach_type_is_unboxed(struct NecroMachProgram* program, NecroMachType* type)
{
    return type->type == program->type_cache.word_int_type->type
        || type->type == program->type_cache.word_float_type->type
        || type->type == program->type_cache.word_uint_type->type
        || type->type == program->type_cache.int32_type->type
        || type->type == program->type_cache.f32_type->type
        || type->type == program->type_cache.int64_type->type
        || type->type == program->type_cache.f64_type->type
        || (type->type == NECRO_MACH_TYPE_STRUCT && type->struct_type.symbol->is_unboxed);
}

bool necro_mach_type_is_unboxed_or_ptr(struct NecroMachProgram* program, NecroMachType* type)
{
    return necro_mach_type_is_unboxed(program, type) || type->type == NECRO_MACH_TYPE_PTR;
}

bool necro_mach_type_is_word_uint(struct NecroMachProgram* program, NecroMachType* type)
{
    return type->type == program->type_cache.word_uint_type->type;
}

NecroMachType* necro_mach_type_make_ptr_if_boxed(NecroMachProgram* program, NecroMachType* type)
{
    if (necro_mach_type_is_unboxed_or_ptr(program, type))
        return type;
    else
        return necro_mach_type_create_ptr(&program->arena, type);
}

size_t necro_mach_word_size_in_bytes(NecroMachProgram* program)
{
    return (program->word_size == NECRO_WORD_4_BYTES) ? 4 : 8;
}

// TODO: Take into account Padding?
size_t necro_mach_type_calculate_size_in_bytes_go(NecroMachType* type, size_t word_size_in_bytes)
{
    assert(type != NULL);
    switch (type->type)
    {
    case NECRO_MACH_TYPE_VOID:   assert(false && "void type cannot be a struct member"); return 0;
    case NECRO_MACH_TYPE_CHAR:   assert(false && "Struct members cannot be smaller than word sized"); return 1;
    case NECRO_MACH_TYPE_UINT1:  assert(false && "Struct members cannot be smaller than word sized"); return 1;
    case NECRO_MACH_TYPE_UINT8:  assert(false && "Struct members cannot be smaller than word sized"); return 1;
    case NECRO_MACH_TYPE_UINT16: assert(false && "Struct members cannot be smaller than word sized"); return 2;
    case NECRO_MACH_TYPE_UINT32: return 4;
    case NECRO_MACH_TYPE_UINT64: return 8;
    case NECRO_MACH_TYPE_INT32:  return 4;
    case NECRO_MACH_TYPE_INT64:  return 8;
    case NECRO_MACH_TYPE_F32:    return 4;
    case NECRO_MACH_TYPE_F64:    return 8;
    case NECRO_MACH_TYPE_PTR:    return word_size_in_bytes;
    case NECRO_MACH_TYPE_ARRAY:
    {
        size_t element_size = necro_mach_type_calculate_size_in_bytes_go(type->array_type.element_type, word_size_in_bytes);
        return element_size * type->array_type.element_count;
    }
    case NECRO_MACH_TYPE_STRUCT:
    {
        size_t struct_size = 0;
        for (size_t i = 0; i < type->struct_type.num_members; ++i)
        {
            struct_size += necro_mach_type_calculate_size_in_bytes_go(type->struct_type.members[i], word_size_in_bytes);
        }
        return struct_size;
    }
    case NECRO_MACH_TYPE_FN:
        assert(false);
        return 0;
    default:
        assert(false);
        return 0;
    }
}

size_t necro_mach_type_calculate_size_in_bytes(NecroMachProgram* program, NecroMachType* type)
{
    return necro_mach_type_calculate_size_in_bytes_go(type, necro_mach_word_size_in_bytes(program));
}

bool necro_mach_type_is_sum_type(NecroMachType* type)
{
    if (type->type == NECRO_MACH_TYPE_PTR)
        type = type->ptr_type.element_type;
    return type->type == NECRO_MACH_TYPE_STRUCT && type->struct_type.sum_type_symbol != NULL;
}

NecroMachType* necro_mach_type_get_sum_type(NecroMachType* type)
{
    assert(necro_mach_type_is_sum_type(type));
    if (type->type == NECRO_MACH_TYPE_PTR)
        type = type->ptr_type.element_type;
    return type->struct_type.sum_type_symbol->mach_type;
}

///////////////////////////////////////////////////////
// Print
///////////////////////////////////////////////////////
void necro_mach_type_print_go(NecroMachType* type, bool is_recursive)
{
    if (type == NULL)
    {
        printf("NULL");
        return;
    }
    switch (type->type)
    {
    case NECRO_MACH_TYPE_VOID:
        printf("void");
        return;
    case NECRO_MACH_TYPE_UINT1:
        printf("u1");
        return;
    case NECRO_MACH_TYPE_UINT8:
        printf("u8");
        return;
    case NECRO_MACH_TYPE_UINT16:
        printf("u16");
        return;
    case NECRO_MACH_TYPE_UINT32:
        printf("u32");
        return;
    case NECRO_MACH_TYPE_UINT64:
        printf("u64");
        return;
    case NECRO_MACH_TYPE_INT32:
        printf("i32");
        return;
    case NECRO_MACH_TYPE_INT64:
        printf("i64");
        return;
    case NECRO_MACH_TYPE_F32:
        printf("f32");
        return;
    case NECRO_MACH_TYPE_F64:
        printf("f64");
        return;
    case NECRO_MACH_TYPE_CHAR:
        printf("char");
        return;
    case NECRO_MACH_TYPE_PTR:
        necro_mach_type_print_go(type->ptr_type.element_type, false);
        printf("*");
        return;
    case NECRO_MACH_TYPE_ARRAY:
        printf("[%zu x ", type->array_type.element_count);
        necro_mach_type_print_go(type->array_type.element_type, false);
        printf("]");
        return;
    case NECRO_MACH_TYPE_STRUCT:
        if (is_recursive)
        {
            printf("%s { ", type->struct_type.symbol->name->str);
            for (size_t i = 0; i < type->struct_type.num_members; ++i)
            {
                necro_mach_type_print_go(type->struct_type.members[i], false);
                if (i < type->struct_type.num_members - 1)
                    printf(", ");
            }
            printf(" }");
        }
        else
        {
            printf("%s", type->struct_type.symbol->name->str);
        }
        return;
    case NECRO_MACH_TYPE_FN:
    {
        printf("fn (");
        for (size_t i = 0; i < type->fn_type.num_parameters; ++i)
        {
            necro_mach_type_print_go(type->fn_type.parameters[i], false);
            if (i < type->fn_type.num_parameters - 1)
                printf(", ");
        }
        printf(") -> ");
        necro_mach_type_print_go(type->fn_type.return_type, false);
        return;
    }
    }
}

void necro_mach_type_print(NecroMachType* type)
{
    necro_mach_type_print_go(type, true);
}

///////////////////////////////////////////////////////
// Type Check
///////////////////////////////////////////////////////
void necro_mach_type_check(NecroMachProgram* program, NecroMachType* type1, NecroMachType* type2)
{
    assert(type1 != NULL);
    assert(type2 != NULL);
    assert(type1->type == type2->type);
    switch (type1->type)
    {
    case NECRO_MACH_TYPE_STRUCT:
        assert(type1->struct_type.symbol == type2->struct_type.symbol);
        return;
    case NECRO_MACH_TYPE_FN:
        assert(type1->fn_type.num_parameters == type2->fn_type.num_parameters);
        for (size_t i = 0; i < type1->fn_type.num_parameters; ++i)
        {
            necro_mach_type_check(program, type1->fn_type.parameters[i], type2->fn_type.parameters[i]);
        }
        necro_mach_type_check(program, type1->fn_type.return_type, type2->fn_type.return_type);
        return;
    case NECRO_MACH_TYPE_PTR:
        necro_mach_type_check(program, type1->ptr_type.element_type, type2->ptr_type.element_type);
        return;
    case NECRO_MACH_TYPE_ARRAY:
        assert(type1->array_type.element_count == type2->array_type.element_count);
        necro_mach_type_check(program, type1->array_type.element_type, type2->array_type.element_type);
        return;
    default:
        return;
    }
}

void necro_mach_type_check_is_int_type(NecroMachType* type1)
{
    assert(
        type1->type == NECRO_MACH_TYPE_INT32 ||
        type1->type == NECRO_MACH_TYPE_INT64
    );
}

void necro_mach_type_check_is_uint_type(NecroMachType* type1)
{
    assert(
        type1->type == NECRO_MACH_TYPE_UINT1  ||
        type1->type == NECRO_MACH_TYPE_UINT8  ||
        type1->type == NECRO_MACH_TYPE_UINT16 ||
        type1->type == NECRO_MACH_TYPE_UINT32 ||
        type1->type == NECRO_MACH_TYPE_UINT64
    );
}

void necro_mach_type_check_is_float_type(NecroMachType* type1)
{
    assert(
        type1->type == NECRO_MACH_TYPE_F32 ||
        type1->type == NECRO_MACH_TYPE_F64
    );
}

// NOTE: Based off of structural equality, not pointer equality
bool necro_mach_type_is_eq(NecroMachType* type1, NecroMachType* type2)
{
    assert(type1 != NULL);
    assert(type2 != NULL);
    if (type1 == type2)
        return true;
    if (type1->type != type2->type)
        return false;
    switch (type1->type)
    {
    case NECRO_MACH_TYPE_STRUCT:
        return type1->struct_type.symbol == type2->struct_type.symbol;
    case NECRO_MACH_TYPE_FN:
        if (type1->fn_type.num_parameters != type2->fn_type.num_parameters)
            return false;
        for (size_t i = 0; i < type1->fn_type.num_parameters; ++i)
        {
            if (!necro_mach_type_is_eq(type1->fn_type.parameters[i], type2->fn_type.parameters[i]))
                return false;
        }
        return necro_mach_type_is_eq(type1->fn_type.return_type, type2->fn_type.return_type);
    case NECRO_MACH_TYPE_PTR:
        return necro_mach_type_is_eq(type1->ptr_type.element_type, type2->ptr_type.element_type);
    case NECRO_MACH_TYPE_ARRAY:
        return type1->array_type.element_count == type2->array_type.element_count && necro_mach_type_is_eq(type1->array_type.element_type, type2->array_type.element_type);
    default:
        return true;
    }
}

void necro_mach_ast_type_check_value(NecroMachProgram* program, NecroMachAst* ast)
{
    assert(ast->type == NECRO_MACH_VALUE);
    assert(ast->necro_machine_type != NULL);
    switch (ast->value.value_type)
    {
    case NECRO_MACH_VALUE_UINT1_LITERAL:  necro_mach_type_check(program, program->type_cache.uint1_type, ast->necro_machine_type);  return;
    case NECRO_MACH_VALUE_UINT8_LITERAL:  necro_mach_type_check(program, program->type_cache.uint8_type, ast->necro_machine_type);  return;
    case NECRO_MACH_VALUE_UINT16_LITERAL: necro_mach_type_check(program, program->type_cache.uint16_type, ast->necro_machine_type); return;
    case NECRO_MACH_VALUE_UINT32_LITERAL: necro_mach_type_check(program, program->type_cache.uint32_type, ast->necro_machine_type); return;
    case NECRO_MACH_VALUE_UINT64_LITERAL: necro_mach_type_check(program, program->type_cache.uint64_type, ast->necro_machine_type); return;
    case NECRO_MACH_VALUE_INT32_LITERAL:  necro_mach_type_check(program, program->type_cache.int32_type, ast->necro_machine_type);  return;
    case NECRO_MACH_VALUE_INT64_LITERAL:  necro_mach_type_check(program, program->type_cache.int64_type, ast->necro_machine_type);  return;
    case NECRO_MACH_VALUE_F32_LITERAL:    necro_mach_type_check(program, program->type_cache.f32_type, ast->necro_machine_type);    return;
    case NECRO_MACH_VALUE_F64_LITERAL:    necro_mach_type_check(program, program->type_cache.f64_type, ast->necro_machine_type);    return;
    case NECRO_MACH_VALUE_VOID:           necro_mach_type_check(program, program->type_cache.void_type, ast->necro_machine_type);   return;
    case NECRO_MACH_VALUE_UNDEFINED:      return;
    default: return;
    }
}

void necro_mach_ast_type_check_block(NecroMachProgram* program, NecroMachAst* ast)
{
    assert(program != NULL);
    while (ast != NULL)
    {
        assert(ast->type == NECRO_MACH_BLOCK);
        assert(ast->block.symbol != NULL);
        assert(ast->block.terminator != NULL);
        assert(ast->block.num_statements <= ast->block.statements_size);
        for (size_t i = 0; i < ast->block.num_statements; ++i)
        {
            necro_mach_ast_type_check(program, ast->block.statements[i]);
        }
        ast = ast->block.next_block;
    }
}

void necro_mach_ast_type_check_call(NecroMachProgram* program, NecroMachAst* ast)
{
    assert(program != NULL);
    assert(ast->type == NECRO_MACH_CALL);
    NecroMachType* fn_value_type = ast->call.fn_value->necro_machine_type;
    assert(fn_value_type->type == NECRO_MACH_TYPE_FN);
    assert(fn_value_type->fn_type.num_parameters == ast->call.num_parameters);
    for (size_t i = 0; i < ast->call.num_parameters; i++)
    {
        necro_mach_ast_type_check(program, ast->call.parameters[i]);
        necro_mach_type_check(program, fn_value_type->fn_type.parameters[i], ast->call.parameters[i]->necro_machine_type);
    }
}

void necro_mach_ast_type_check_call_intrinsic(NecroMachProgram* program, NecroMachAst* ast)
{
    assert(program != NULL);
    assert(ast->type == NECRO_MACH_CALLI);
    NecroMachType* fn_value_type = ast->call_intrinsic.intrinsic_type;
    assert(fn_value_type->type == NECRO_MACH_TYPE_FN);
    assert(fn_value_type->fn_type.num_parameters == ast->call_intrinsic.num_parameters);
    for (size_t i = 0; i < ast->call_intrinsic.num_parameters; i++)
    {
        necro_mach_ast_type_check(program, ast->call_intrinsic.parameters[i]);
        necro_mach_type_check(program, fn_value_type->fn_type.parameters[i], ast->call_intrinsic.parameters[i]->necro_machine_type);
    }
}

void necro_mach_ast_type_check_load(NecroMachProgram* program, NecroMachAst* ast)
{
    assert(program != NULL);
    assert(ast->type == NECRO_MACH_LOAD);
    necro_mach_ast_type_check(program, ast->load.dest_value);
    necro_mach_ast_type_check(program, ast->load.source_ptr);
    NecroMachAst* source_ptr_ast = ast->load.source_ptr;
    assert(source_ptr_ast->type == NECRO_MACH_VALUE);
    assert(source_ptr_ast->necro_machine_type->type == NECRO_MACH_TYPE_PTR);
    assert(source_ptr_ast->value.value_type == NECRO_MACH_VALUE_REG || source_ptr_ast->value.value_type == NECRO_MACH_VALUE_PARAM || source_ptr_ast->value.value_type == NECRO_MACH_VALUE_GLOBAL);
    necro_mach_type_check(program, source_ptr_ast->necro_machine_type->ptr_type.element_type, ast->load.dest_value->necro_machine_type);
}

void necro_mach_ast_type_check_store(NecroMachProgram* program, NecroMachAst* ast)
{
    assert(program != NULL);
    assert(ast->type == NECRO_MACH_STORE);
    necro_mach_ast_type_check(program, ast->store.source_value);
    necro_mach_ast_type_check(program, ast->store.dest_ptr);
    NecroMachAst* source_value = ast->store.source_value;
    NecroMachAst* dest_ptr     = ast->store.dest_ptr;
    assert(source_value->type == NECRO_MACH_VALUE);
    assert(dest_ptr->type == NECRO_MACH_VALUE);
    assert(dest_ptr->necro_machine_type->type == NECRO_MACH_TYPE_PTR);
    necro_mach_type_check(program, source_value->necro_machine_type, dest_ptr->necro_machine_type->ptr_type.element_type);
}

// void necro_mach_ast_type_check_nalloc(NecroMachProgram* program, NecroMachAst* ast)
// {
//     assert(program != NULL);
//     assert(ast->type == NECRO_MACH_NALLOC);
//     assert(ast->nalloc.type_to_alloc != NULL);
//     assert(ast->nalloc.slots_used > 0);
//     necro_mach_ast_type_check(program, ast->nalloc.result_reg);
//     assert(ast->nalloc.result_reg->necro_machine_type->type == NECRO_MACH_TYPE_PTR);
//     necro_mach_type_check(program, ast->nalloc.type_to_alloc, ast->nalloc.result_reg->necro_machine_type->ptr_type.element_type);
// }

void necro_mach_ast_type_check_bit_cast(NecroMachProgram* program, NecroMachAst* ast)
{
    assert(program != NULL);
    assert(ast->type == NECRO_MACH_BIT_CAST);
    assert(ast->bit_cast.to_value->type == NECRO_MACH_VALUE);
    assert(ast->bit_cast.to_value->value.value_type == NECRO_MACH_VALUE_REG);
    necro_mach_ast_type_check(program, ast->bit_cast.from_value);
    necro_mach_ast_type_check(program, ast->bit_cast.to_value);
    // TODO: Check that they are part of the same union type?
}

void necro_mach_ast_type_check_zext(NecroMachProgram* program, NecroMachAst* ast)
{
    assert(program != NULL);
    necro_mach_ast_type_check(program, ast->zext.from_value);
    necro_mach_ast_type_check(program, ast->zext.to_value);
    NecroMachAst*  value   = ast->zext.from_value;
    NecroMachType* to_type = ast->zext.to_value->necro_machine_type;
    assert(
        value->necro_machine_type->type == NECRO_MACH_TYPE_UINT1  ||
        value->necro_machine_type->type == NECRO_MACH_TYPE_UINT8  ||
        value->necro_machine_type->type == NECRO_MACH_TYPE_UINT16 ||
        value->necro_machine_type->type == NECRO_MACH_TYPE_UINT32 ||
        value->necro_machine_type->type == NECRO_MACH_TYPE_UINT64 ||
        value->necro_machine_type->type == NECRO_MACH_TYPE_INT32  ||
        value->necro_machine_type->type == NECRO_MACH_TYPE_INT64
    );
    assert(
        to_type->type == NECRO_MACH_TYPE_UINT1  ||
        to_type->type == NECRO_MACH_TYPE_UINT8  ||
        to_type->type == NECRO_MACH_TYPE_UINT16 ||
        to_type->type == NECRO_MACH_TYPE_UINT32 ||
        to_type->type == NECRO_MACH_TYPE_UINT64 ||
        to_type->type == NECRO_MACH_TYPE_INT32  ||
        to_type->type == NECRO_MACH_TYPE_INT64
    );
}

void necro_mach_ast_type_check_gep(NecroMachProgram* program, NecroMachAst* ast)
{
    assert(program != NULL);
    NecroMachAst* source_value = ast->gep.source_value;
    NecroMachAst* dest_value   = ast->gep.dest_value;
    necro_mach_ast_type_check(program, ast->gep.source_value);
    necro_mach_ast_type_check(program, ast->gep.dest_value);
    assert(source_value->type == NECRO_MACH_VALUE);
    assert(dest_value->type == NECRO_MACH_VALUE);
    NecroMachType* necro_machine_type = source_value->necro_machine_type;
    for (size_t i = 0; i < ast->gep.num_indices; ++i)
    {
        if (necro_machine_type->type == NECRO_MACH_TYPE_STRUCT)
        {
            NecroMachAst* index_ast = ast->gep.indices[i];
            necro_mach_ast_type_check(program, index_ast);
            assert(index_ast->type == NECRO_MACH_VALUE);
            // if (program->word_size == NECRO_WORD_4_BYTES)
                // assert(index_ast->value.value_type == NECRO_MACH_VALUE_UINT32_LITERAL);
            // else
            assert(index_ast->value.value_type == NECRO_MACH_VALUE_UINT64_LITERAL);
            const size_t index = index_ast->value.uint64_literal;
            assert(index < necro_machine_type->struct_type.num_members);
            necro_machine_type = necro_machine_type->struct_type.members[index];
        }
        else if (necro_machine_type->type == NECRO_MACH_TYPE_ARRAY)
        {
            NecroMachAst* index_ast = ast->gep.indices[i];
            necro_mach_ast_type_check(program, index_ast);
            assert(index_ast->type == NECRO_MACH_VALUE);
            // Can't double check this with non-const geps!
            // assert(index_ast->value.value_type == NECRO_MACH_VALUE_UINT32_LITERAL);
            // const size_t index = index_ast->value.uint64_literal;
            // assert(index < necro_machine_type->array_type.element_count);
            necro_machine_type = necro_machine_type->array_type.element_type;
        }
        else if (necro_machine_type->type == NECRO_MACH_TYPE_PTR)
        {
            assert(i == 0); // NOTE: Can only deref the first type!
            necro_machine_type = necro_machine_type->ptr_type.element_type;
        }
        else
        {
            assert(false && "gep type error");
        }
    }
}

void necro_mach_ast_type_check_insert_value(NecroMachProgram* program, NecroMachAst* ast)
{
    NecroMachAst* aggregate_value = ast->insert_value.aggregate_value;
    NecroMachAst* inserted_value  = ast->insert_value.inserted_value;
    size_t        index           = ast->insert_value.index;
    NecroMachAst* dest_value      = ast->insert_value.dest_value;
    assert(program != NULL);
    assert(aggregate_value->type == NECRO_MACH_VALUE);
    assert(aggregate_value->necro_machine_type->type == NECRO_MACH_TYPE_STRUCT ||
           aggregate_value->necro_machine_type->type == NECRO_MACH_TYPE_ARRAY);
    assert(inserted_value->type == NECRO_MACH_VALUE);
    if (aggregate_value->necro_machine_type->type == NECRO_MACH_TYPE_STRUCT)
    {
        assert(index < aggregate_value->necro_machine_type->struct_type.num_members);
        assert(necro_mach_type_is_eq(aggregate_value->necro_machine_type, dest_value->necro_machine_type));
        assert(necro_mach_type_is_eq(aggregate_value->necro_machine_type->struct_type.members[index], inserted_value->necro_machine_type));
    }
    else
    {
        assert(index < aggregate_value->necro_machine_type->array_type.element_count);
        assert(necro_mach_type_is_eq(aggregate_value->necro_machine_type, dest_value->necro_machine_type));
        assert(necro_mach_type_is_eq(aggregate_value->necro_machine_type->array_type.element_type, inserted_value->necro_machine_type));
    }
    necro_mach_ast_type_check(program, ast->insert_value.aggregate_value);
    necro_mach_ast_type_check(program, ast->insert_value.inserted_value);
    necro_mach_ast_type_check(program, ast->insert_value.dest_value);
}

void necro_mach_ast_type_check_extract_value(NecroMachProgram* program, NecroMachAst* ast)
{
    NecroMachAst* aggregate_value = ast->extract_value.aggregate_value;
    size_t        index           = ast->extract_value.index;
    NecroMachAst* dest_value      = ast->extract_value.dest_value;
    assert(program != NULL);
    assert(aggregate_value->type == NECRO_MACH_VALUE);
    assert(aggregate_value->necro_machine_type->type == NECRO_MACH_TYPE_STRUCT ||
           aggregate_value->necro_machine_type->type == NECRO_MACH_TYPE_ARRAY);
    if (aggregate_value->necro_machine_type->type == NECRO_MACH_TYPE_STRUCT)
    {
        assert(index < aggregate_value->necro_machine_type->struct_type.num_members);
        assert(necro_mach_type_is_eq(aggregate_value->necro_machine_type->struct_type.members[index], dest_value->necro_machine_type));
    }
    else
    {
        assert(index < aggregate_value->necro_machine_type->array_type.element_count);
        assert(necro_mach_type_is_eq(aggregate_value->necro_machine_type->array_type.element_type, dest_value->necro_machine_type));
    }
    necro_mach_ast_type_check(program, ast->extract_value.aggregate_value);
    necro_mach_ast_type_check(program, ast->extract_value.dest_value);
}

void necro_mach_ast_type_check_binop(NecroMachProgram* program, NecroMachAst* ast)
{
    assert(program != NULL);
    NecroMachAst* left   = ast->binop.left;
    NecroMachAst* right  = ast->binop.right;
    NecroMachAst* result = ast->binop.result;
    necro_mach_ast_type_check(program, left);
    necro_mach_ast_type_check(program, right);
    necro_mach_ast_type_check(program, result);

	switch (ast->binop.binop_type)
	{
    case NECRO_PRIMOP_BINOP_FSHL: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_FSHR:
		break;
	default:
		necro_mach_type_check(program, left->necro_machine_type, right->necro_machine_type);
		break;
	}

    switch (ast->binop.binop_type)
    {
    case NECRO_PRIMOP_BINOP_IADD: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_ISUB: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_IMUL: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_IDIV:
    case NECRO_PRIMOP_BINOP_IREM:
    {
        necro_mach_type_check_is_int_type(left->necro_machine_type);
        necro_mach_type_check_is_int_type(right->necro_machine_type);
        necro_mach_type_check_is_int_type(result->necro_machine_type);
        necro_mach_type_check_is_int_type(ast->necro_machine_type);
        break;
    }
    case NECRO_PRIMOP_BINOP_UADD: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_USUB: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_UMUL: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_UDIV: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_UREM: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_OR:   /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_AND:  /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_SHL:  /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_SHR:  /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_XOR:
    {
        necro_mach_type_check_is_uint_type(left->necro_machine_type);
        necro_mach_type_check_is_uint_type(right->necro_machine_type);
        necro_mach_type_check_is_uint_type(result->necro_machine_type);
        necro_mach_type_check_is_uint_type(ast->necro_machine_type);
        break;
    }
    case NECRO_PRIMOP_BINOP_FSHL: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_FSHR:
    {
        necro_mach_type_check_is_float_type(left->necro_machine_type);
        necro_mach_type_check_is_uint_type(right->necro_machine_type);
        necro_mach_type_check_is_float_type(result->necro_machine_type);
        necro_mach_type_check_is_float_type(ast->necro_machine_type);
        break;
    }
    case NECRO_PRIMOP_BINOP_FADD: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_FSUB: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_FMUL: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_FDIV: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_FREM: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_FAND: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_FOR:  /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_FXOR:
    {
        necro_mach_type_check_is_float_type(left->necro_machine_type);
        necro_mach_type_check_is_float_type(right->necro_machine_type);
        necro_mach_type_check_is_float_type(result->necro_machine_type);
        necro_mach_type_check_is_float_type(ast->necro_machine_type);
        break;
    }
    default:
        assert(false);
    }
    assert(result->type == NECRO_MACH_VALUE);
    assert(result->value.value_type == NECRO_MACH_VALUE_REG);
}

void necro_mach_ast_type_check_uop(NecroMachProgram* program, NecroMachAst* ast)
{
    assert(program != NULL);
    assert(ast->type == NECRO_MACH_UOP);
    NecroMachAst* param  = ast->uop.param;
    NecroMachAst* result = ast->uop.result;
    necro_mach_ast_type_check(program, param);
    necro_mach_ast_type_check(program, result);
    assert(param->type == NECRO_MACH_VALUE);
    assert(result->type == NECRO_MACH_VALUE);
    switch (ast->uop.uop_type)
    {
    case NECRO_PRIMOP_UOP_ITOI: /* FALL THROUGH */
    case NECRO_PRIMOP_UOP_IABS: /* FALL THROUGH */
    case NECRO_PRIMOP_UOP_ISGN:
    {
        necro_mach_type_check_is_int_type(param->necro_machine_type);
        necro_mach_type_check_is_int_type(result->necro_machine_type);
        break;
    }
    case NECRO_PRIMOP_UOP_UABS: /* FALL THROUGH */
    case NECRO_PRIMOP_UOP_NOT:  /* FALL THROUGH */
    case NECRO_PRIMOP_UOP_USGN:
    {
        necro_mach_type_check_is_uint_type(param->necro_machine_type);
        necro_mach_type_check_is_uint_type(result->necro_machine_type);
        break;
    }
    case NECRO_PRIMOP_UOP_FSGN: /* FALL THROUGH */
    case NECRO_PRIMOP_UOP_FNOT: /* FALL THROUGH */
    case NECRO_PRIMOP_UOP_FBREV:
    {
        necro_mach_type_check_is_float_type(param->necro_machine_type);
        necro_mach_type_check_is_float_type(result->necro_machine_type);
        break;
    }
    case NECRO_PRIMOP_UOP_FTOB:
    {
        necro_mach_type_check_is_float_type(param->necro_machine_type);
        necro_mach_type_check_is_uint_type(result->necro_machine_type);
        break;
    }
    case NECRO_PRIMOP_UOP_FFRB:
    {
        necro_mach_type_check_is_uint_type(param->necro_machine_type);
        necro_mach_type_check_is_float_type(result->necro_machine_type);
        break;
    }
    case NECRO_PRIMOP_UOP_ITOU:
    {
        necro_mach_type_check_is_int_type(param->necro_machine_type);
        necro_mach_type_check_is_uint_type(result->necro_machine_type);
        break;
    }
    case NECRO_PRIMOP_UOP_ITOF:
    {
        necro_mach_type_check_is_int_type(param->necro_machine_type);
        necro_mach_type_check_is_float_type(result->necro_machine_type);
        break;
    }
    case NECRO_PRIMOP_UOP_UTOI:
    {
        necro_mach_type_check_is_uint_type(param->necro_machine_type);
        necro_mach_type_check_is_int_type(result->necro_machine_type);
        break;
    }
    case NECRO_PRIMOP_UOP_FTRI:
    {
        necro_mach_type_check_is_float_type(param->necro_machine_type);
        necro_mach_type_check_is_int_type(result->necro_machine_type);
        break;
    }
    case NECRO_PRIMOP_UOP_FTRU:
    {
        necro_mach_type_check_is_float_type(param->necro_machine_type);
        necro_mach_type_check_is_uint_type(result->necro_machine_type);
        break;
    }
    case NECRO_PRIMOP_UOP_FRNI:
    {
        necro_mach_type_check_is_float_type(param->necro_machine_type);
        necro_mach_type_check_is_int_type(result->necro_machine_type);
        break;
    }
    case NECRO_PRIMOP_UOP_FTOF:
    {
        necro_mach_type_check_is_float_type(param->necro_machine_type);
        necro_mach_type_check_is_float_type(result->necro_machine_type);
        break;
    }
    case NECRO_PRIMOP_UOP_FFLR:
    {
        necro_mach_type_is_eq(program->type_cache.f64_type, param->necro_machine_type);
        necro_mach_type_is_eq(program->type_cache.f64_type, result->necro_machine_type);
        break;
    }
    default:
        assert(false);
    }
    assert(ast->uop.result->type == NECRO_MACH_VALUE);
    assert(ast->uop.result->value.value_type == NECRO_MACH_VALUE_REG);
}

void necro_mach_ast_type_check_cmp(NecroMachProgram* program, NecroMachAst* ast)
{
    assert(program != NULL);
    assert(ast->type == NECRO_MACH_CMP);
    NecroMachAst* left   = ast->cmp.left;
    NecroMachAst* right  = ast->cmp.right;
    NecroMachAst* result = ast->cmp.result;
    necro_mach_ast_type_check(program, left);
    necro_mach_ast_type_check(program, right);
    necro_mach_ast_type_check(program, result);
    assert(left != NULL);
    assert(left->type == NECRO_MACH_VALUE);
    assert(right != NULL);
    assert(right->type == NECRO_MACH_VALUE);
    assert(
        left->necro_machine_type->type == NECRO_MACH_TYPE_UINT1  ||
        left->necro_machine_type->type == NECRO_MACH_TYPE_UINT8  ||
        left->necro_machine_type->type == NECRO_MACH_TYPE_UINT16 ||
        left->necro_machine_type->type == NECRO_MACH_TYPE_UINT32 ||
        left->necro_machine_type->type == NECRO_MACH_TYPE_UINT64 ||
        left->necro_machine_type->type == NECRO_MACH_TYPE_INT32  ||
        left->necro_machine_type->type == NECRO_MACH_TYPE_INT64  ||
        left->necro_machine_type->type == NECRO_MACH_TYPE_F32    ||
        left->necro_machine_type->type == NECRO_MACH_TYPE_F64    ||
        left->necro_machine_type->type == NECRO_MACH_TYPE_PTR);
    assert(left->necro_machine_type->type == right->necro_machine_type->type);
    assert(ast->cmp.result->type == NECRO_MACH_VALUE);
    assert(ast->cmp.result->value.value_type == NECRO_MACH_VALUE_REG);
}

void necro_mach_ast_type_check_phi(NecroMachProgram* program, NecroMachAst* ast)
{
    assert(program != NULL);
    assert(ast->type == NECRO_MACH_PHI);
    NecroMachAst* result = ast->phi.result;
    necro_mach_ast_type_check(program, result);
    assert(result->type == NECRO_MACH_VALUE);
    assert(result->value.value_type == NECRO_MACH_VALUE_REG);
    NecroMachPhiList* phi_list = ast->phi.values;
    while (phi_list != NULL)
    {
        NecroMachAst* phi_value = phi_list->data.value;
        necro_mach_ast_type_check(program, phi_value);
        assert(phi_value->type == NECRO_MACH_VALUE);
        necro_mach_type_check(program, result->necro_machine_type, phi_value->necro_machine_type);
        phi_list = phi_list->next;
    }
}

void necro_mach_ast_type_check_memcpy(NecroMachProgram* program, NecroMachAst* ast)
{
    UNUSED(program);
    UNUSED(ast);
    // TODO:
}

void necro_mach_ast_type_check_memset(NecroMachProgram* program, NecroMachAst* ast)
{
    UNUSED(program);
    UNUSED(ast);
    // TODO:
}

void necro_mach_ast_type_check_struct_def(NecroMachProgram* program, NecroMachAst* ast)
{
    UNUSED(program);
    UNUSED(ast);
    // TODO:
}

void necro_mach_ast_type_check_fn_def(NecroMachProgram* program, NecroMachAst* ast)
{
    assert(program != NULL);
    assert(ast->type == NECRO_MACH_FN_DEF);
    if (ast->fn_def.fn_type == NECRO_MACH_FN_RUNTIME)
    {
        assert(ast->fn_def.runtime_fn_addr != NULL);
    }
    else
    {
        necro_mach_ast_type_check(program, ast->fn_def.call_body);
        assert(ast->fn_def.call_body->type == NECRO_MACH_BLOCK);
    }
}

void necro_mach_ast_type_check_mach_def(NecroMachProgram* program, NecroMachAst* ast)
{
    assert(program != NULL);
    assert(ast->type == NECRO_MACH_DEF);
    assert(ast->machine_def.update_fn != NULL);
    if (ast->machine_def.mk_fn != NULL)
    {
        assert(ast->machine_def.init_fn != NULL);
        necro_mach_ast_type_check(program, ast->machine_def.mk_fn);
        necro_mach_ast_type_check(program, ast->machine_def.init_fn);
    }
    necro_mach_ast_type_check(program, ast->machine_def.update_fn);
}

void necro_mach_ast_type_check(NecroMachProgram* program, NecroMachAst* ast)
{
    assert(ast != NULL);
    switch(ast->type)
    {
    case NECRO_MACH_VALUE:         necro_mach_ast_type_check_value(program, ast);          return;
    case NECRO_MACH_BLOCK:         necro_mach_ast_type_check_block(program, ast);          return;
    case NECRO_MACH_CALL:          necro_mach_ast_type_check_call(program, ast);           return;
    case NECRO_MACH_CALLI:         necro_mach_ast_type_check_call_intrinsic(program, ast); return;
    case NECRO_MACH_LOAD:          necro_mach_ast_type_check_load(program, ast);           return;
    case NECRO_MACH_STORE:         necro_mach_ast_type_check_store(program, ast);          return;
    case NECRO_MACH_BIT_CAST:      necro_mach_ast_type_check_bit_cast(program, ast);       return;
    case NECRO_MACH_ZEXT:          necro_mach_ast_type_check_zext(program, ast);           return;
    case NECRO_MACH_GEP:           necro_mach_ast_type_check_gep(program, ast);            return;
    case NECRO_MACH_EXTRACT_VALUE: necro_mach_ast_type_check_extract_value(program, ast);  return;
    case NECRO_MACH_INSERT_VALUE:  necro_mach_ast_type_check_insert_value(program, ast);   return;
    case NECRO_MACH_BINOP:         necro_mach_ast_type_check_binop(program, ast);          return;
    case NECRO_MACH_UOP:           necro_mach_ast_type_check_uop(program, ast);            return;
    case NECRO_MACH_CMP:           necro_mach_ast_type_check_cmp(program, ast);            return;
    case NECRO_MACH_PHI:           necro_mach_ast_type_check_phi(program, ast);            return;
    case NECRO_MACH_MEMCPY:        necro_mach_ast_type_check_memcpy(program, ast);         return;
    case NECRO_MACH_MEMSET:        necro_mach_ast_type_check_memset(program, ast);         return;
    case NECRO_MACH_STRUCT_DEF:    necro_mach_ast_type_check_struct_def(program, ast);     return;
    case NECRO_MACH_FN_DEF:        necro_mach_ast_type_check_fn_def(program, ast);         return;
    case NECRO_MACH_DEF:           necro_mach_ast_type_check_mach_def(program, ast);       return;
    default:
        assert(false && "Unrecognized ast type in necro_mach_ast_type_check");
        return;
    }
}


// TODO: Register block path verification
void necro_mach_program_verify(NecroMachProgram* program)
{
    // TODO: More verification
    // globals
    for (size_t i = 0; i < program->globals.length; ++i)
    {
        necro_mach_ast_type_check(program, program->globals.data[i]);
    }
    // functions
    for (size_t i = 0; i < program->functions.length; ++i)
    {
        necro_mach_ast_type_check(program, program->functions.data[i]);
    }
    // machine defs
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        necro_mach_ast_type_check(program, program->machine_defs.data[i]);
    }
    // main
    necro_mach_ast_type_check(program, program->necro_main);
}

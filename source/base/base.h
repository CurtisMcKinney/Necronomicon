/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_BASE_H
#define NECRO_BASE_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "utility.h"
#include "ast.h"
#include "symtable.h"

// TODO: Proxy type?

#define NECRO_MAX_UNBOXED_TUPLE_TYPES 8
#define NECRO_MAX_ENV_TYPES 16
#define NECRO_MAX_BRANCH_TYPES 16

typedef struct NecroBase
{
    NecroAstArena ast;

    NecroAstSymbol* higher_kind; // NOTE: Not actually supported, only here for error messages
    NecroAstSymbol* kind_kind;   // NOTE: Instead of Kind of Type being NULL or Type
    NecroAstSymbol* star_kind;
    NecroAstSymbol* attribute_kind;
    NecroAstSymbol* nat_kind;
    NecroAstSymbol* sym_kind;

    // TODO: Change to uniqueness
    // Ownership
    NecroAstSymbol* ownership_kind;
    NecroAstSymbol* ownership_share;
    NecroAstSymbol* ownership_steal;
    // NecroAstSymbol* share_type;

    NecroAstSymbol* tuple2_con;
    NecroAstSymbol* tuple3_con;
    NecroAstSymbol* tuple4_con;
    NecroAstSymbol* tuple5_con;
    NecroAstSymbol* tuple6_con;
    NecroAstSymbol* tuple7_con;
    NecroAstSymbol* tuple8_con;
    NecroAstSymbol* tuple9_con;
    NecroAstSymbol* tuple10_con;

    NecroAstSymbol* tuple2_type;
    NecroAstSymbol* tuple3_type;
    NecroAstSymbol* tuple4_type;
    NecroAstSymbol* tuple5_type;
    NecroAstSymbol* tuple6_type;
    NecroAstSymbol* tuple7_type;
    NecroAstSymbol* tuple8_type;
    NecroAstSymbol* tuple9_type;
    NecroAstSymbol* tuple10_type;

    NecroAstSymbol*  unboxed_tuple_types[NECRO_MAX_UNBOXED_TUPLE_TYPES];
    NecroAstSymbol*  unboxed_tuple_cons[NECRO_MAX_UNBOXED_TUPLE_TYPES];
    NecroAstSymbol*  env_types[NECRO_MAX_ENV_TYPES];
    NecroAstSymbol*  env_cons[NECRO_MAX_ENV_TYPES];
    NecroAstSymbol*  branch_types[NECRO_MAX_BRANCH_TYPES];
    NecroAstSymbol** branch_cons[NECRO_MAX_BRANCH_TYPES];

    NecroAstSymbol* world_type;
    NecroAstSymbol* unit_type;
    NecroAstSymbol* unit_con;
    NecroAstSymbol* seq_type;
    NecroAstSymbol* seq_con;
    NecroAstSymbol* seq_value_type;
    NecroAstSymbol* seq_param_type;
    // NecroAstSymbol* list_type;
    NecroAstSymbol* int_type;
    NecroAstSymbol* int_con;
    // NecroAstSymbol* int64_type;
    NecroAstSymbol* uint_type;
    NecroAstSymbol* float_type;
    NecroAstSymbol* float_con;
    NecroAstSymbol* float_vec;
    // NecroAstSymbol* float64_type;
    NecroAstSymbol* rational_type;
    NecroAstSymbol* char_type;
    NecroAstSymbol* char_con;
    NecroAstSymbol* bool_type;
    NecroAstSymbol* true_con;
    NecroAstSymbol* false_con;
    // NecroAstSymbol* fractional_type_class;
    NecroAstSymbol* semi_ring_class;
    NecroAstSymbol* ring_class;
    NecroAstSymbol* division_ring_class;
    NecroAstSymbol* euclidean_ring_class;
    NecroAstSymbol* field_class;
    NecroAstSymbol* num_type_class;
    NecroAstSymbol* integral_class;
    NecroAstSymbol* bits_class;
    NecroAstSymbol* floating_class;
    NecroAstSymbol* eq_type_class;
    NecroAstSymbol* ord_type_class;
    NecroAstSymbol* functor_type_class;
    NecroAstSymbol* applicative_type_class;
    NecroAstSymbol* monad_type_class;
    NecroAstSymbol* default_type_class;
    NecroAstSymbol* enum_type_class;
    NecroAstSymbol* audio_type_class;
    NecroAstSymbol* mono_type;
    NecroAstSymbol* prev_fn;
    NecroAstSymbol* ptr_type;
    NecroAstSymbol* array_type;
    NecroAstSymbol* range_type;
    NecroAstSymbol* range_con;
    NecroAstSymbol* index_type;
    NecroAstSymbol* index_con;
    NecroAstSymbol* unsafe_empty_array;
    NecroAstSymbol* read_array;
    NecroAstSymbol* read_arrayu;
    NecroAstSymbol* write_array;
    NecroAstSymbol* prim_undefined;
    NecroAstSymbol* proj_fn;
    NecroAstSymbol* block_size_type;
    NecroAstSymbol* sample_rate_type;
    NecroAstSymbol* nat_mul_type;
    NecroAstSymbol* nat_div_type;
    NecroAstSymbol* nat_max_type;
    NecroAstSymbol* nat_next_power_of_2;
    NecroAstSymbol* sample_rate;
    NecroAstSymbol* recip_sample_rate;

    NecroAstSymbol* pipe_forward;
    NecroAstSymbol* pipe_back;
    NecroAstSymbol* compose_forward;
    NecroAstSymbol* compose_back;
    NecroAstSymbol* from_int;
    NecroAstSymbol* run_seq;
    NecroAstSymbol* seq_tick;
    NecroAstSymbol* tuple_tick;
    NecroAstSymbol* interleave_tick;

    // Runtime functions
    NecroAstSymbol* test_assertion;
    NecroAstSymbol* panic;
    NecroAstSymbol* mouse_x_fn;
    NecroAstSymbol* mouse_y_fn;
    NecroAstSymbol* keyPress_fn;
    NecroAstSymbol* ptr_malloc;
    // NecroAstSymbol* unsafe_peek;
    // NecroAstSymbol* unsafe_poke;
    // NecroAstSymbol* unsafe_free;
    NecroAstSymbol* print_int;
    NecroAstSymbol* print_uint;
    // NecroAstSymbol* print_i64;
    NecroAstSymbol* print_float;
    NecroAstSymbol* print_char;
    NecroAstSymbol* out_audio_block;
    NecroAstSymbol* record_audio_block;
    NecroAstSymbol* record_audio_block_finalize;
    NecroAstSymbol* audio_file_open;

    NecroAstSymbol* close_file;
    NecroAstSymbol* write_int_to_file;
    NecroAstSymbol* write_uint_to_file;
    NecroAstSymbol* write_float_to_file;
    NecroAstSymbol* write_char_to_file;
    NecroAstSymbol* open_file;

    NecroAstSymbol* print_audio_block;
    NecroAstSymbol* fast_floor;
    NecroAstSymbol* ceil_float;
    NecroAstSymbol* truncate_float;
    NecroAstSymbol* round_float;
    NecroAstSymbol* copy_sign_float;
    NecroAstSymbol* min_float;
    NecroAstSymbol* max_float;
    NecroAstSymbol* smin;
    NecroAstSymbol* smax;
    NecroAstSymbol* umin;
    NecroAstSymbol* umax;
    NecroAstSymbol* floor_to_int_float;
    NecroAstSymbol* ceil_to_int_float;
    NecroAstSymbol* truncate_to_int_float;
    NecroAstSymbol* round_to_int_float;
    NecroAstSymbol* fma;
    NecroAstSymbol* bit_reverse_uint;
    NecroAstSymbol* bit_and_float;
    NecroAstSymbol* bit_not_float;
    NecroAstSymbol* bit_or_float;
    NecroAstSymbol* bit_xor_float;
    NecroAstSymbol* bit_shift_left_float;
    NecroAstSymbol* bit_shift_right_float;
    NecroAstSymbol* bit_shift_right_a_float;
    NecroAstSymbol* bit_reverse_float;
    NecroAstSymbol* to_bits_float;
    NecroAstSymbol* from_bits_float;
    NecroAstSymbol* floor_float;
    NecroAstSymbol* audio_sample_offset;
    NecroAstSymbol* abs_float;
    NecroAstSymbol* sine_float;
    NecroAstSymbol* cosine_float;
    NecroAstSymbol* exp_float;
    NecroAstSymbol* exp2_float;
    NecroAstSymbol* log_float;
    NecroAstSymbol* log10_float;
    NecroAstSymbol* log2_float;
    NecroAstSymbol* pow_float;
    NecroAstSymbol* sqrt_float;
    NecroScopedSymTable* scoped_symtable;

} NecroBase;

NecroBase           necro_base_compile(NecroIntern* intern, NecroScopedSymTable* scoped_symtable);
void                necro_base_destroy(NecroBase* base);
void                necro_base_global_init();
void                necro_base_global_cleanup();
void                necro_base_test();
NecroAstSymbol*     necro_base_get_tuple_type(NecroBase* base, size_t num);
NecroAstSymbol*     necro_base_get_tuple_con(NecroBase* base, size_t num);
NecroAstSymbol*     necro_base_get_unboxed_tuple_type(NecroBase* base, size_t num);
NecroAstSymbol*     necro_base_get_unboxed_tuple_con(NecroBase* base, size_t num);
NecroAstSymbol*     necro_base_get_env_type(NecroBase* base, size_t num);
NecroAstSymbol*     necro_base_get_env_con(NecroBase* base, size_t num);
NecroAstSymbol*     necro_base_get_branch_type(NecroBase* base, size_t branch_size);
NecroAstSymbol*     necro_base_get_branch_con(NecroBase* base, size_t branch_size, size_t alternative);
NecroCoreAstSymbol* necro_base_get_proj_symbol(NecroPagedArena* arena, NecroBase* base);
bool                necro_base_is_nat_op_type(const NecroBase* base, const NecroType* type);
#endif // NECRO_BASE_H

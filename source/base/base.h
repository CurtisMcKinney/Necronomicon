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

struct NecroMachProgram;

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

    NecroAstSymbol*  env_types[NECRO_MAX_ENV_TYPES];
    NecroAstSymbol*  env_cons[NECRO_MAX_ENV_TYPES];
    NecroAstSymbol*  branch_types[NECRO_MAX_BRANCH_TYPES];
    NecroAstSymbol** branch_cons[NECRO_MAX_BRANCH_TYPES];

    NecroAstSymbol* poly_type;
    NecroAstSymbol* world_type;
    NecroAstSymbol* unit_type;
    NecroAstSymbol* unit_con;
    // NecroAstSymbol* list_type;
    NecroAstSymbol* int_type;
    NecroAstSymbol* uint_type;
    NecroAstSymbol* float_type;
    NecroAstSymbol* audio_type;
    NecroAstSymbol* rational_type;
    NecroAstSymbol* char_type;
    NecroAstSymbol* bool_type;
    NecroAstSymbol* true_con;
    NecroAstSymbol* false_con;
    NecroAstSymbol* num_type_class;
    NecroAstSymbol* fractional_type_class;
    NecroAstSymbol* eq_type_class;
    NecroAstSymbol* ord_type_class;
    NecroAstSymbol* functor_type_class;
    NecroAstSymbol* applicative_type_class;
    NecroAstSymbol* monad_type_class;
    NecroAstSymbol* default_type_class;
    NecroAstSymbol* prev_fn;
    NecroAstSymbol* event_type;
    NecroAstSymbol* pattern_type;
    NecroAstSymbol* closure_type;
    NecroAstSymbol* ptr_type;
    NecroAstSymbol* array_type;
    NecroAstSymbol* range_type;
    NecroAstSymbol* index_type;
    NecroAstSymbol* maybe_type;
    NecroAstSymbol* prim_undefined;

    // Runtime functions
    NecroAstSymbol* mouse_x_fn;
    NecroAstSymbol* mouse_y_fn;
    NecroAstSymbol* unsafe_malloc;
    NecroAstSymbol* unsafe_peek;
    NecroAstSymbol* unsafe_poke;
    NecroAstSymbol* print_int;

    NecroScopedSymTable* scoped_symtable;

} NecroBase;

NecroBase       necro_base_compile(NecroIntern* intern, NecroScopedSymTable* scoped_symtable);
void            necro_base_init_mach(struct NecroMachProgram* program, NecroBase* base);
void            necro_base_destroy(NecroBase* base);
void            necro_base_test();
NecroAstSymbol* necro_base_get_tuple_type(NecroBase* base, size_t num);
NecroAstSymbol* necro_base_get_tuple_con(NecroBase* base, size_t num);
NecroAstSymbol* necro_base_get_env_type(NecroBase* base, size_t num);
NecroAstSymbol* necro_base_get_env_con(NecroBase* base, size_t num);
NecroAstSymbol* necro_base_get_branch_type(NecroBase* base, size_t branch_size);
NecroAstSymbol* necro_base_get_branch_con(NecroBase* base, size_t branch_size, size_t alternative);

#endif // NECRO_BASE_H
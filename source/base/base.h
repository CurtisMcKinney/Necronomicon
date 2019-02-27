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

typedef struct NecroBase
{
    NecroAstArena ast;

    NecroAstSymbol* higher_kind; // NOTE: Not actually supported, only here for error messages
    NecroAstSymbol* kind_kind; // NOTE: Instead of Kind of Type being NULL or Type
    NecroAstSymbol* star_kind;
    NecroAstSymbol* nat_kind;
    NecroAstSymbol* sym_kind;

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

    NecroAstSymbol* poly_type;
    NecroAstSymbol* world_type;
    NecroAstSymbol* world_value;
    NecroAstSymbol* unit_type;
    NecroAstSymbol* unit_con;
    // NecroAstSymbol* list_type;
    NecroAstSymbol* int_type;
    NecroAstSymbol* float_type;
    NecroAstSymbol* audio_type;
    NecroAstSymbol* rational_type;
    NecroAstSymbol* char_type;
    NecroAstSymbol* bool_type;
    NecroAstSymbol* num_type_class;
    NecroAstSymbol* fractional_type_class;
    NecroAstSymbol* eq_type_class;
    NecroAstSymbol* ord_type_class;
    NecroAstSymbol* functor_type_class;
    NecroAstSymbol* applicative_type_class;
    NecroAstSymbol* monad_type_class;
    NecroAstSymbol* default_type_class;
    NecroAstSymbol* event_type;
    NecroAstSymbol* pattern_type;
    NecroAstSymbol* closure_type;
    // NecroAstSymbol* dyn_state_type;
    // NecroAstSymbol* apply_fn;
    NecroAstSymbol* ptr_type;
    NecroAstSymbol* array_type;
    NecroAstSymbol* maybe_type;
    NecroAstSymbol* prim_undefined;

    // Runtime functions
    NecroAstSymbol* mouse_x_fn;
    NecroAstSymbol* mouse_y_fn;
    NecroAstSymbol* unsafe_malloc;
    NecroAstSymbol* unsafe_peek;
    NecroAstSymbol* unsafe_poke;

} NecroBase;

NecroBase necro_base_compile(NecroIntern* intern, NecroScopedSymTable* scoped_symtable);
void      necro_base_destroy(NecroBase* base);
void      necro_base_test();

#endif // NECRO_BASE_H
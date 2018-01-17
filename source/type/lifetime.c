/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include "symtable.h"
#include "lifetime.h"

NecroLifetime* necro_create_constant_lifetime(NecroInfer* infer)
{
    NecroSymbol     constant_symbol = necro_intern_string(infer->intern, "\'constant");
    NecroSymbolInfo constant_info   = necro_create_initial_symbol_info(constant_symbol, (NecroSourceLoc) { 0 }, NULL, infer->intern);
    NecroID         constant_id     = necro_symtable_insert(infer->symtable, constant_info);
    NecroCon        constant_con    = (NecroCon) { .id = constant_id, .symbol = constant_symbol };
    NecroType*      constant_type   = necro_alloc_type(infer);
    constant_type->type             = NECRO_TYPE_CON;
    constant_type->con              = (NecroTypeCon)
    {
        .con      = constant_con,
        .args     = NULL,
        .arity    = 0,
        .is_class = false,
    };
    constant_type->type_kind    = NULL;
    constant_type->pre_supplied = true;
    assert(necro_symtable_get(infer->symtable, constant_con.id)->type == NULL);
    necro_symtable_get(infer->symtable, constant_con.id)->type = constant_type;
    return constant_type;
}

NecroLifetime* necro_create_static_lifetime(NecroInfer* infer)
{
    NecroSymbol     static_symbol = necro_intern_string(infer->intern, "\'static");
    NecroSymbolInfo static_info   = necro_create_initial_symbol_info(static_symbol, (NecroSourceLoc) { 0 }, NULL, infer->intern);
    NecroID         static_id     = necro_symtable_insert(infer->symtable, static_info);
    NecroCon        static_con    = (NecroCon) { .id = static_id, .symbol = static_symbol };
    NecroType*      static_type   = necro_alloc_type(infer);
    static_type->type             = NECRO_TYPE_CON;
    static_type->con              = (NecroTypeCon)
    {
        .con      = static_con,
        .args     = NULL,
        .arity    = 0,
        .is_class = false,
    };
    static_type->type_kind    = NULL;
    static_type->pre_supplied = true;
    assert(necro_symtable_get(infer->symtable, static_con.id)->type == NULL);
    necro_symtable_get(infer->symtable, static_con.id)->type = static_type;
    return static_type;
}

NecroLifetime* necro_create_never_lifetime(NecroInfer* infer)
{
    NecroSymbol     never_symbol = necro_intern_string(infer->intern, "\'never");
    NecroSymbolInfo never_info   = necro_create_initial_symbol_info(never_symbol, (NecroSourceLoc) { 0 }, NULL, infer->intern);
    NecroID         never_id     = necro_symtable_insert(infer->symtable, never_info);
    NecroCon        never_con    = (NecroCon) { .id = never_id, .symbol = never_symbol };
    NecroType*      never_type   = necro_alloc_type(infer);
    never_type->type             = NECRO_TYPE_CON;
    never_type->con              = (NecroTypeCon)
    {
        .con      = never_con,
        .args     = NULL,
        .arity    = 0,
        .is_class = false,
    };
    never_type->type_kind    = NULL;
    never_type->pre_supplied = true;
    assert(necro_symtable_get(infer->symtable, never_con.id)->type == NULL);
    necro_symtable_get(infer->symtable, never_con.id)->type = never_type;
    return never_type;
}

void necro_infer_lifetime(NecroInfer* infer, NecroASTNode* ast)
{

}

// Unifies towards longer lifetime
// Will need a second version that unifies towards minimum lifetime for scope introducing combinators (or a special rule enforced somehow...)
void necro_unify_lifetimes(NecroInfer* infer, NecroLifetime* lifetime1, NecroLifetime* lifetime2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    assert(infer != NULL);
    assert(lifetime1 != NULL);
    assert(lifetime2 != NULL);
    lifetime1 = necro_find(infer, lifetime1);
    lifetime2 = necro_find(infer, lifetime2);
    if (lifetime1 == lifetime2)
        return;
    switch (lifetime1->type)
    {
    // TODO: Finish
    // case NECRO_TYPE_VAR:  necro_kind_unify_var(infer, lifetime1, lifetime2, scope, macro_type, error_preamble); return;
    // case NECRO_TYPE_CON:  necro_kind_unify_con(infer, lifetime1, lifetime2, scope, macro_type, error_preamble); return;
    case NECRO_TYPE_FOR:
    {
        while (lifetime1->type == NECRO_TYPE_FOR)
            lifetime1 = lifetime1->for_all.type;
        necro_unify_lifetimes(infer, lifetime1, lifetime2, scope, macro_type, error_preamble);
        return;
    }

    case NECRO_TYPE_APP:  necro_infer_error(infer, error_preamble, lifetime1, "Compiler bug: Found lifetime app in necro_unify_lifetimes"); return;
    case NECRO_TYPE_FUN:  necro_infer_error(infer, error_preamble, lifetime1, "Compiler bug: Found lifetime function in necro_unify_lifetimes"); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, error_preamble, lifetime1, "Compiler bug: Found lifetime args list in necro_unify_lifetimes"); return;
    default: necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Non-existent lifetime node type (lifetime1: %d, lifetime2: %s) found in necro_unify_lifetimes.", lifetime1->type, lifetime2->type); return;
    }
}
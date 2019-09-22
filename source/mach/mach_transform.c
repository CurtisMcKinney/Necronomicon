/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "mach_transform.h"
#include "mach_print.h"
#include "alias_analysis.h"
#include "type/monomorphize.h"
#include "core/core_infer.h"
#include "core/lambda_lift.h"
#include "core/defunctionalization.h"
#include "infer.h"
#include <ctype.h>
#include "core/state_analysis.h"
#include "mach_case.h"


/*
    TODO:
        * Array empty + ops
        * Handle deep_copy_fn with arrays!
        * For loop initializer handling!
        * Test stateful for loops which need initializers and stateful recursive shit in for loops which need initializers
        * Exhaustive Case expression / Redundant Case Expression
        * llvm allocator?
        * llvm codegen
        * wildcard flag in NecroCoreAstVar
        * bind_rec (for mutual recursion)
        * Somehow we have a memory leak?
        * Defunctionalization
*/


///////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////
NecroMachAst* necro_mach_value_get_state_ptr(NecroMachAst* fn_def)
{
    // assert(fn_def->fn_def.state_ptr != NULL);
    return fn_def->fn_def.state_ptr;
}

void necro_mach_value_put_state_ptr(NecroMachAst* fn_def, NecroMachAst* state_ptr)
{
    // assert(state_ptr != NULL);
    fn_def->fn_def.state_ptr = state_ptr;
}

///////////////////////////////////////////////////////
// Pass 1
///////////////////////////////////////////////////////
NecroMachAst* necro_core_transform_to_mach_1_data_con_sum_type(NecroMachProgram* program, NecroMachAstSymbol* mach_ast_symbol, size_t max_arg_count)
{
    NecroArenaSnapshot snapshot = necro_snapshot_arena_get(&program->snapshot_arena);
    NecroMachType**    members  = necro_snapshot_arena_alloc(&program->snapshot_arena, (1 + max_arg_count) * sizeof(NecroMachType*));
    members[0]                  = program->type_cache.word_uint_type;
    for (size_t i = 1; i < max_arg_count + 1; ++i)
    {
        members[i] = program->type_cache.word_uint_type;
    }
    NecroMachAst* struct_def = necro_mach_create_struct_def(program, mach_ast_symbol, members, max_arg_count + 1);
    necro_snapshot_arena_rewind(&program->snapshot_arena, snapshot);
    return struct_def;
}

NecroMachAst* necro_core_transform_to_mach_1_data_con_type(NecroMachProgram* program, NecroMachAstSymbol* mach_ast_symbol, NecroCoreAst* core_ast, NecroMachAstSymbol* sum_type_symbol)
{
    NecroArenaSnapshot snapshot = necro_snapshot_arena_get(&program->snapshot_arena);
    assert(core_ast->ast_type == NECRO_CORE_AST_DATA_CON);
    NecroType*      con_type  = necro_type_strip_for_all(core_ast->data_con.type);
    assert(con_type->type == NECRO_TYPE_CON || con_type->type == NECRO_TYPE_FUN);
    const size_t    arg_count = necro_type_arity(con_type);
    NecroMachType** members   = necro_snapshot_arena_alloc(&program->snapshot_arena, (1 + arg_count) * sizeof(NecroMachType*));
    members[0]                = program->type_cache.word_uint_type;
    for (size_t i = 1; i < arg_count + 1; ++i)
    {
        assert(con_type->type == NECRO_TYPE_FUN);
        members[i] = necro_mach_type_make_ptr_if_boxed(program, necro_mach_type_from_necro_type(program, con_type->fun.type1));
        con_type   = con_type->fun.type2;
    }
    NecroMachAst* struct_def = necro_mach_create_struct_def_with_sum_type(program, mach_ast_symbol, members, arg_count + 1, sum_type_symbol);
    necro_snapshot_arena_rewind(&program->snapshot_arena, snapshot);
    return struct_def;
}

void necro_core_transform_to_mach_1_data_con_constructor(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachType* struct_type, NecroMachType* con_struct_type, size_t con_number)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_DATA_CON);
    assert(con_struct_type != NULL);
    assert(con_struct_type->type == NECRO_MACH_TYPE_STRUCT);
    assert(struct_type != NULL);
    assert(struct_type->type == NECRO_MACH_TYPE_STRUCT);

    NecroArenaSnapshot  snapshot        = necro_snapshot_arena_get(&program->snapshot_arena);
    NecroMachType*      struct_ptr_type = necro_mach_type_create_ptr(&program->arena, struct_type);
    NecroMachType*      con_ptr_type    = necro_mach_type_create_ptr(&program->arena, con_struct_type);
    NecroMachAstSymbol* con_symbol      = necro_mach_ast_symbol_gen(program, NULL, necro_snapshot_arena_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_mk", core_ast->data_con.ast_symbol->name->str }), NECRO_MANGLE_NAME);
    core_ast->data_con.ast_symbol->mach_symbol = con_symbol;
    NecroType*          con_type        = necro_type_strip_for_all(core_ast->data_con.type);
    assert(con_type != NULL && (con_type->type == NECRO_TYPE_CON || con_type->type == NECRO_TYPE_FUN));
    const size_t        arg_count       = necro_type_arity(con_type);

    NecroMachType**     parameters      = necro_snapshot_arena_alloc(&program->snapshot_arena, (1 + arg_count) * sizeof(NecroMachType*));
    parameters[0]                       = con_ptr_type;
    for (size_t i = 1; i < arg_count + 1; ++i)
    {
        assert(con_type->type == NECRO_TYPE_FUN);
        parameters[i] = necro_mach_type_make_ptr_if_boxed(program, necro_mach_type_from_necro_type(program, con_type->fun.type1));
        con_type      = con_type->fun.type2;
    }
    NecroMachType* mk_fn_type = necro_mach_type_create_fn(&program->arena, struct_ptr_type, parameters, arg_count + 1);
    NecroMachAst*  mk_fn_body = necro_mach_block_create(program, "entry", NULL);
    NecroMachAst*  mk_fn_def  = necro_mach_create_fn(program, con_symbol, mk_fn_body, mk_fn_type);
    const size_t   slots_used = arg_count + 1;
    assert(slots_used <= UINT32_MAX);
    NecroMachAst*  data_ptr   = necro_mach_value_get_state_ptr(mk_fn_def);
    NecroMachAst*  tag_ptr    = necro_mach_build_gep(program, mk_fn_def, data_ptr, (uint32_t[]) { 0, 0 }, 2, "tag");
    necro_mach_build_store(program, mk_fn_def, necro_mach_value_create_word_uint(program, con_number), tag_ptr);

    //--------------
    // Parameters
    for (size_t i = 0; i < arg_count; ++i)
    {
        NecroMachAst* param_reg = necro_mach_value_create_param_reg(program, mk_fn_def, i + 1);
        NecroMachAst* slot_ptr  = necro_mach_build_gep(program, mk_fn_def, data_ptr, (uint32_t[]) { 0, i + 1 }, 2, "slot");
        necro_mach_build_store(program, mk_fn_def, param_reg, slot_ptr);
    }

    if (struct_type == con_struct_type)
    {
        necro_mach_build_return(program, mk_fn_def, data_ptr);
    }
    else
    {
        NecroMachAst* cast_ptr = necro_mach_build_up_cast(program, mk_fn_def, data_ptr, necro_mach_type_create_ptr(&program->arena, struct_type));
        necro_mach_build_return(program, mk_fn_def, cast_ptr);
    }
    con_symbol->ast                            = mk_fn_def->fn_def.fn_value;
    con_symbol->state_type                     = NECRO_STATE_POLY;
    con_symbol->is_enum                        = false;
    con_symbol->is_constructor                 = true;
    con_symbol->is_primitive                   = false;
    con_symbol->con_num                        = con_number;
    core_ast->data_con.ast_symbol->mach_symbol = con_symbol;
    con_symbol->mach_type                      = mk_fn_def->necro_machine_type;
    necro_snapshot_arena_rewind(&program->snapshot_arena, snapshot);
}

void necro_core_transform_to_mach_1_data_decl(NecroMachProgram* program, NecroCoreAst* core_ast)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_DATA_DECL);

    NecroArenaSnapshot snapshot = necro_snapshot_arena_get(&program->snapshot_arena);

    //--------------
    // Skip primitives
    if (core_ast->data_decl.ast_symbol->mach_symbol != NULL && core_ast->data_decl.ast_symbol->is_primitive)
    {
        necro_snapshot_arena_rewind(&program->snapshot_arena, snapshot);
        return;
    }

    //--------------
    // Prune polymorphic types
    if (necro_type_is_polymorphic_ignoring_ownership(program->base, core_ast->data_decl.ast_symbol->type))
    {
        necro_snapshot_arena_rewind(&program->snapshot_arena, snapshot);
        return;
    }

    //--------------
    // Get max args
    size_t            max_arg_count = 0;
    NecroCoreAstList* cons          = core_ast->data_decl.con_list;
    while (cons != NULL)
    {
        NecroCoreAst* con       = cons->data;
        assert(con->ast_type == NECRO_CORE_AST_DATA_CON);
        // Prune polymorphic types
        if (necro_type_is_polymorphic_ignoring_ownership(program->base, con->data_con.type))
        {
            necro_snapshot_arena_rewind(&program->snapshot_arena, snapshot);
            return;
        }
        NecroType*    con_type  = necro_type_strip_for_all(con->data_con.type);
        assert(con_type->type == NECRO_TYPE_CON || con_type->type == NECRO_TYPE_FUN);
        const size_t  arg_count = necro_type_arity(con_type);
        max_arg_count           = (arg_count > max_arg_count) ? arg_count : max_arg_count;
        cons                    = cons->next;
    }

    core_ast->data_decl.ast_symbol->mach_symbol = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, core_ast->data_decl.ast_symbol);

    //--------------
    // Enum type early exit:
    if (max_arg_count == 0)
    {
        core_ast->data_decl.ast_symbol->mach_symbol->mach_type = program->type_cache.word_uint_type;
        cons                                                   = core_ast->data_decl.con_list;
        while (cons != NULL)
        {
            NecroCoreAst* con = cons->data;
            assert(con->ast_type == NECRO_CORE_AST_DATA_CON);
            con->data_con.ast_symbol->mach_symbol                  = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, core_ast->data_con.ast_symbol);
            con->data_con.ast_symbol->mach_symbol->is_constructor  = true;
            con->data_con.ast_symbol->mach_symbol->is_enum         = true;
            core_ast->data_decl.ast_symbol->mach_symbol->mach_type = program->type_cache.word_uint_type;
            cons                                                   = cons->next;
        }
        necro_snapshot_arena_rewind(&program->snapshot_arena, snapshot);
        return;
    }

    //--------------
    // Create struct_defs
    NecroMachAst* struct_def = NULL;
    const bool   is_sum_type = core_ast->data_decl.con_list->next != NULL;
    if (is_sum_type)
    {
        struct_def = necro_core_transform_to_mach_1_data_con_sum_type(program, core_ast->data_decl.ast_symbol->mach_symbol, max_arg_count);
        cons       = core_ast->data_decl.con_list;
        while (cons != NULL)
        {
            cons->data->data_con.ast_symbol->mach_symbol = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, cons->data->data_con.ast_symbol);
            necro_core_transform_to_mach_1_data_con_type(program, cons->data->data_con.ast_symbol->mach_symbol, cons->data, struct_def->struct_def.symbol);
            cons = cons->next;
        }
    }
    else
    {
        core_ast->data_decl.con_list->data->data_con.ast_symbol->mach_symbol = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, core_ast->data_con.ast_symbol);
        struct_def                                                           = necro_core_transform_to_mach_1_data_con_type(program, core_ast->data_decl.ast_symbol->mach_symbol, core_ast->data_decl.con_list->data, NULL);
    }

    //--------------
    // Create constructor fn_defs
    cons = core_ast->data_decl.con_list;
    size_t con_number = 0;
    while (cons != NULL)
    {
        necro_core_transform_to_mach_1_data_con_constructor(program, cons->data, struct_def->necro_machine_type, cons->data->data_con.ast_symbol->mach_symbol->mach_type, con_number);
        con_number++;
        cons = cons->next;
    }

    necro_snapshot_arena_rewind(&program->snapshot_arena, snapshot);
}

void necro_core_transform_to_mach_1_bind(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_BIND);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);

    if (core_ast->bind.ast_symbol->is_primitive)
        return;

    NecroArenaSnapshot snapshot = necro_snapshot_arena_get(&program->snapshot_arena);

    //---------------
    // Create initial MachineDef
    NecroMachAst* machine_def = necro_mach_create_initial_machine_def(program, necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, core_ast->bind.ast_symbol), outer, necro_mach_type_from_necro_type(program, core_ast->bind.ast_symbol->type), core_ast->bind.ast_symbol->type);
    // machine_def->machine_def.is_recursive = core_ast->bind.is_recursive; // TODO: Use bind_rec for recursive nodes!

    //---------------
    // Count and assign arg names
    size_t num_args = necro_core_ast_num_args(core_ast);
    if (num_args > 0)
    {
        machine_def->machine_def.num_arg_names = num_args;
        machine_def->machine_def.arg_names     = necro_paged_arena_alloc(&program->arena, num_args * sizeof(NecroMachAstSymbol));
        num_args                               = 0;
        NecroCoreAst* lambdas                  = core_ast->bind.expr;
        while (lambdas->ast_type == NECRO_CORE_AST_LAM)
        {
            machine_def->machine_def.arg_names[num_args] = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, lambdas->lambda.arg->var.ast_symbol);
            num_args++;
            lambdas = lambdas->lambda.expr;
        }
    }

    //---------------
    // go deeper
    if (core_ast->bind.initializer != NULL)
    {
        necro_core_transform_to_mach_1_go(program, core_ast->bind.initializer, machine_def);
    }
    necro_core_transform_to_mach_1_go(program, core_ast->bind.expr, machine_def);

    //---------------
    // declare mk_fn and init_fn
    if (outer == NULL)
    {
        // mk_fn
        {
            NecroMachAstSymbol* mk_fn_symbol = necro_mach_ast_symbol_gen(program, NULL, necro_snapshot_arena_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "mk", machine_def->machine_def.machine_name->name->str + 1 }), NECRO_MANGLE_NAME);
            NecroMachType*      mk_fn_type   = necro_mach_type_create_fn(&program->arena, necro_mach_type_create_void(program), NULL, 0);
            NecroMachAst*       mk_fn_body   = necro_mach_block_create(program, "entry", NULL);
            NecroMachAst*       mk_fn_def    = necro_mach_create_fn(program, mk_fn_symbol, mk_fn_body, mk_fn_type);
            program->functions.length--; // HACK: Don't want the mk function in the functions list, instead it belongs to the machine
            machine_def->machine_def.mk_fn   = mk_fn_def;
        }

        // init_fn
        {
            NecroMachAstSymbol* init_fn_symbol = necro_mach_ast_symbol_gen(program, NULL, necro_snapshot_arena_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "init", machine_def->machine_def.machine_name->name->str + 1 }), NECRO_MANGLE_NAME);
            NecroMachType*      init_fn_type   = necro_mach_type_create_fn(&program->arena, necro_mach_type_create_void(program), (NecroMachType*[]) { program->type_cache.word_uint_type }, 1);
            NecroMachAst*       init_fn_body   = necro_mach_block_create(program, "entry", NULL);
            NecroMachAst*       init_fn_def    = necro_mach_create_fn(program, init_fn_symbol, init_fn_body, init_fn_type);
            program->functions.length--; // HACK: Don't want the mk function in the functions list, instead it belongs to the machine
            machine_def->machine_def.init_fn   = init_fn_def;
        }
    }

    necro_snapshot_arena_rewind(&program->snapshot_arena, snapshot);
}

void necro_core_transform_to_mach_1_bind_rec(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_BIND_REC);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);
}

void necro_core_transform_to_mach_1_app(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_APP);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);
    necro_core_transform_to_mach_1_go(program, core_ast->app.expr1, outer);
    necro_core_transform_to_mach_1_go(program, core_ast->app.expr2, outer);
}

void necro_core_transform_to_mach_1_lam(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_LAM);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);
    necro_core_transform_to_mach_1_go(program, core_ast->lambda.arg, outer);
    necro_core_transform_to_mach_1_go(program, core_ast->lambda.expr, outer);
}

void necro_core_transform_to_mach_1_let(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_LET);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);
    necro_core_transform_to_mach_1_go(program, core_ast->let.bind, outer);
    necro_core_transform_to_mach_1_go(program, core_ast->let.expr, outer);
}

void necro_core_transform_to_mach_1_var(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_VAR);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);
    core_ast->var.ast_symbol->mach_symbol = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, core_ast->var.ast_symbol);
}

void necro_core_transform_to_mach_1_lit(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_LIT);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);
    if (core_ast->lit.type != NECRO_AST_CONSTANT_ARRAY)
        return;
    NecroCoreAstList* elements = core_ast->lit.array_literal_elements;
    while (elements != NULL)
    {
        necro_core_transform_to_mach_1_go(program, elements->data, outer);
        elements = elements->next;
    }
}

void necro_core_transform_to_mach_1_for(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_FOR);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);
    necro_core_transform_to_mach_1_go(program, core_ast->for_loop.range_init, outer);
    necro_core_transform_to_mach_1_go(program, core_ast->for_loop.value_init, outer);
    necro_core_transform_to_mach_1_go(program, core_ast->for_loop.index_arg, outer);
    necro_core_transform_to_mach_1_go(program, core_ast->for_loop.value_arg, outer);
    necro_core_transform_to_mach_1_go(program, core_ast->for_loop.expression, outer);
}

void necro_core_transform_to_mach_1_go(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    switch (core_ast->ast_type)
    {
    case NECRO_CORE_AST_LIT:       necro_core_transform_to_mach_1_lit(program, core_ast, outer);return;
    case NECRO_CORE_AST_VAR:       necro_core_transform_to_mach_1_var(program, core_ast, outer); return;
    case NECRO_CORE_AST_BIND:      necro_core_transform_to_mach_1_bind(program, core_ast, outer); return;
    case NECRO_CORE_AST_BIND_REC:  necro_core_transform_to_mach_1_bind_rec(program, core_ast, outer); return;
    case NECRO_CORE_AST_APP:       necro_core_transform_to_mach_1_app(program, core_ast, outer); return;
    case NECRO_CORE_AST_LAM:       necro_core_transform_to_mach_1_lam(program, core_ast, outer); return;
    case NECRO_CORE_AST_LET:       necro_core_transform_to_mach_1_let(program, core_ast, outer); return;
    case NECRO_CORE_AST_CASE:      necro_core_transform_to_mach_1_case(program, core_ast, outer); return;
    case NECRO_CORE_AST_CASE_ALT:  assert(false && "found NECRO_CORE_AST_CASE_ALT in necro_core_transform_to_mach_1_go"); return;
    case NECRO_CORE_AST_FOR:       necro_core_transform_to_mach_1_for(program, core_ast, outer); return;
    case NECRO_CORE_AST_DATA_DECL: necro_core_transform_to_mach_1_data_decl(program, core_ast); return;
    case NECRO_CORE_AST_DATA_CON:  assert(false && "found NECRO_CORE_AST_DATA_CON in necro_core_transform_to_mach_1_go"); return;
    default:                       assert(false && "Unimplemented AST type in necro_core_transform_to_mach_1_go"); return;
    }
}


///////////////////////////////////////////////////////
// Pass 2
///////////////////////////////////////////////////////
void necro_core_transform_to_mach_2_let(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_LET);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);
    necro_core_transform_to_mach_2_go(program, core_ast->let.bind, outer);
    necro_core_transform_to_mach_2_go(program, core_ast->let.expr, outer);
}

void necro_core_transform_to_mach_2_lam(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_LAM);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);
    necro_core_transform_to_mach_2_go(program, core_ast->lambda.arg, outer);
    necro_core_transform_to_mach_2_go(program, core_ast->lambda.expr, outer);
}

bool necro_mach_is_var_machine_arg(NecroMachAst* mach_ast, NecroSymbol var_name)
{
    for (size_t i = 0; i < mach_ast->machine_def.num_arg_names; i++)
    {
        if (mach_ast->machine_def.arg_names[i]->name == var_name)
            return true;
    }
    return false;
}

NecroMachSlot necro_mach_add_member_full(NecroMachProgram* program, NecroMachDef* machine_def, NecroMachType* new_member, NecroMachAst* member_ast, NecroMachAst* const_init_value)
{
    assert(program != NULL);
    assert(machine_def != NULL);
    if (machine_def->num_members >= machine_def->members_size)
    {
        NecroMachSlot* old_members = machine_def->members;
        machine_def->members       = necro_paged_arena_alloc(&program->arena, machine_def->members_size * 2 * sizeof(NecroMachSlot));
        memcpy(machine_def->members, old_members, machine_def->members_size * sizeof(NecroMachSlot));
        machine_def->members_size *= 2;
    }
    NecroMachSlot slot                             = (NecroMachSlot) { .necro_machine_type = new_member, .slot_num = machine_def->num_members, .slot_ast = member_ast, .machine_def = machine_def, .const_init_value = const_init_value };
    machine_def->members[machine_def->num_members] = slot;
    machine_def->num_members++;
    return slot;
}

NecroMachSlot necro_mach_add_member(NecroMachProgram* program, NecroMachDef* machine_def, NecroMachType* new_member, NecroMachAst* member_ast)
{
    assert(program != NULL);
    assert(machine_def != NULL);
    return necro_mach_add_member_full(program, machine_def, new_member, member_ast, NULL);
}

void necro_mach_remove_only_self_recursive_member(NecroMachProgram* program, NecroMachAst* ast)
{
    assert(program != NULL);
    assert(ast->type == NECRO_MACH_DEF);
    // TODO:  eexamine this
    // if (ast->machine_def.num_arg_names == 0 || ast->machine_def.num_members != 1 || !necro_mach_type_is_eq(ast->machine_def.members[0].necro_machine_type->ptr_type.element_type, ast->necro_machine_type))
    //     return;
    // ast->machine_def.num_members = 0;
}

void necro_mach_calculate_statefulness(NecroMachProgram* program, NecroMachAst* ast)
{
    assert(program != NULL);
    assert(ast->type == NECRO_MACH_DEF);
    ast->machine_def.state_type = ast->machine_def.machine_name->state_type;
    if (ast->machine_def.state_type == NECRO_STATE_POLY)
    {
        if (ast->machine_def.num_arg_names > 0)
            ast->machine_def.state_type = NECRO_STATE_POINTWISE;
        else
            ast->machine_def.state_type = NECRO_STATE_CONSTANT;
    }
    if (ast->machine_def.update_fn != NULL)
    {
        ast->machine_def.update_fn->fn_def.state_type = ast->machine_def.state_type;
    }
}

void necro_core_transform_to_mach_2_initializer_and_bound_expression(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(core_ast->ast_type == NECRO_CORE_AST_BIND);

    //--------------------
    // Non-recursive (early exit)
    if (core_ast->bind.initializer == NULL)
    {
        necro_core_transform_to_mach_2_go(program, core_ast->bind.expr, outer);
        return;
    }

    assert(outer != NULL);

    //--------------------
    // Initializer
    necro_core_transform_to_mach_2_go(program, core_ast->bind.initializer, outer);

    //--------------------
    // Persistent ptr slot
    NecroMachType* persistent_type = necro_mach_type_make_ptr_if_boxed(program, necro_mach_type_from_necro_type(program, core_ast->bind.initializer->necro_type));
    core_ast->persistent_slot      = necro_mach_add_member(program, &outer->machine_def, persistent_type, NULL).slot_num;

    //--------------------
    // Expr
    necro_core_transform_to_mach_2_go(program, core_ast->bind.expr, outer);
}

void necro_core_transform_to_mach_2_bind(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_BIND);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);

    if (core_ast->bind.ast_symbol->is_primitive)
        return;

    //--------------------
    // Retrieve machine_def
    NecroMachAst* machine_def = core_ast->bind.ast_symbol->mach_symbol->ast;
    assert(machine_def != NULL);
    assert(machine_def->type == NECRO_MACH_DEF);

    //--------------------
    // Go deeper
    if (outer != NULL)
    {
        necro_core_transform_to_mach_2_initializer_and_bound_expression(program, core_ast, outer);
        necro_mach_calculate_statefulness(program, machine_def);
        return;
    }
    machine_def->necro_machine_type = necro_mach_type_create_struct(&program->arena, machine_def->machine_def.machine_name, NULL, 0);
    necro_core_transform_to_mach_2_initializer_and_bound_expression(program, core_ast, machine_def);

    //--------------------
    // Calculate statefulness
    necro_mach_remove_only_self_recursive_member(program, machine_def);
    necro_mach_calculate_statefulness(program, machine_def);

    //--------------------
    // Create State type
    const size_t    num_machine_members  = machine_def->machine_def.num_members;
    NecroMachType** machine_type_members = necro_paged_arena_alloc(&program->arena, num_machine_members * sizeof(NecroMachType*));
    for (size_t i = 0; i < machine_def->machine_def.num_members; ++i)
        machine_type_members[i] = machine_def->machine_def.members[i].necro_machine_type;
    machine_def->necro_machine_type->struct_type.members     = machine_type_members;
    machine_def->necro_machine_type->struct_type.num_members = num_machine_members;

    //--------------------
    // Add global value
    const bool is_machine_fn = machine_def->machine_def.num_arg_names > 0;
    if (outer == NULL && !is_machine_fn)
    {
        NecroMachAst*       global_value  = NULL;
        NecroMachAstSymbol* global_symbol = necro_mach_ast_symbol_gen(program, NULL, necro_snapshot_arena_concat_strings(&program->snapshot_arena, 2, (const char* []) { "global", machine_def->machine_def.machine_name->name->str + 1 }), NECRO_MANGLE_NAME);
        if (necro_mach_type_is_unboxed(program, machine_def->machine_def.value_type))
            global_value = necro_mach_value_create_global(program, global_symbol, necro_mach_type_create_ptr(&program->arena, machine_def->machine_def.value_type));
        else
            global_value = necro_mach_value_create_global(program, global_symbol, necro_mach_type_create_ptr(&program->arena, necro_mach_type_create_ptr(&program->arena, machine_def->machine_def.value_type)));
        machine_def->machine_def.global_value = global_value;
        necro_mach_program_add_global(program, global_value);
    }

    // if (machine_def->machine_def.state_type < NECRO_STATE_STATEFUL)
    if (machine_def->machine_def.num_members == 0)
    {
        machine_def->machine_def.mk_fn   = NULL;
        machine_def->machine_def.init_fn = NULL;
    }

    //--------------------
    // Define mk_fn and init_fn
    // TODO: Initializers
    if (machine_def->machine_def.mk_fn != NULL)
    {
        NecroMachType* machine_ptr_type = necro_mach_type_create_ptr(&program->arena, machine_def->necro_machine_type);

        //--------------------
        // init_fn
        // TODO: Make sure to init for loop state arrays and rec state arrays correctly! Right now we're not handling them at all!
        {
            machine_def->machine_def.init_fn->necro_machine_type->fn_type.parameters[0] = machine_ptr_type;
            NecroMachAst* data_ptr = necro_mach_value_create_param_reg(program, machine_def->machine_def.init_fn, 0);
            // gep and init
            for (size_t i = 0; i < machine_def->machine_def.num_members; ++i)
            {
                NecroMachSlot slot = machine_def->machine_def.members[i];
                if (slot.slot_ast != NULL && slot.slot_ast->type == NECRO_MACH_DEF && slot.slot_ast->machine_def.init_fn != NULL)
                {
                    // FnDef init
                    NecroMachAst* member = necro_mach_build_gep(program, machine_def->machine_def.init_fn, data_ptr, (uint32_t[]) { 0, (uint32_t) i }, 2, "member");
                    necro_mach_build_call(program, machine_def->machine_def.init_fn, slot.slot_ast->machine_def.init_fn->fn_def.fn_value, (NecroMachAst*[]) { member }, 1, NECRO_MACH_CALL_LANG, "");
                }
                else if (slot.const_init_value != NULL)
                {
                    // Const value init
                    NecroMachAst* member = necro_mach_build_gep(program, machine_def->machine_def.init_fn, data_ptr, (uint32_t[]) { 0, (uint32_t) i }, 2, "const_member");
                    necro_mach_build_store(program, machine_def->machine_def.init_fn, slot.const_init_value, member);
                }
            }
            // NOTE: The return_void is added during necro_core_transform_to_mach_3_bind
        }

        //--------------------
        // mk_fn
        {
            machine_def->machine_def.mk_fn->necro_machine_type->fn_type.return_type = machine_ptr_type;
            NecroMachAst* data_ptr = necro_mach_build_nalloc(program, machine_def->machine_def.mk_fn, machine_def->necro_machine_type);
            necro_mach_build_call(program, machine_def->machine_def.mk_fn, machine_def->machine_def.init_fn->fn_def.fn_value, (NecroMachAst*[]) { data_ptr }, 1, NECRO_MACH_CALL_LANG, "");
            necro_mach_build_return(program, machine_def->machine_def.mk_fn, data_ptr);
        }
    }
}

void necro_core_transform_to_mach_2_bind_rec(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_BIND_REC);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);
    assert(false && "TODO: Finish!");
}

void necro_core_transform_to_mach_2_var(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_VAR);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);
    NecroCoreAstSymbol* ast_symbol  = core_ast->var.ast_symbol;
    NecroMachAstSymbol* mach_symbol = ast_symbol->mach_symbol;
    if (mach_symbol->is_enum || ast_symbol->is_primitive)
        return;
    NecroMachAst* machine_ast = mach_symbol->ast;
    if (machine_ast == NULL)
    {
        // This is likely an argument or a pattern variable, ignore
        return;
    }
    if (machine_ast->type == NECRO_MACH_DEF)
    {
        return;
    }
    else if (mach_symbol->is_constructor)
    {
        assert(mach_symbol->ast->necro_machine_type->type == NECRO_MACH_TYPE_FN);
        assert(mach_symbol->ast->necro_machine_type->fn_type.num_parameters == 1);
        assert(mach_symbol->ast->necro_machine_type->fn_type.parameters[0]->type == NECRO_MACH_TYPE_PTR);
        NecroMachType* con_type = mach_symbol->ast->necro_machine_type->fn_type.parameters[0]->ptr_type.element_type;
        core_ast->persistent_slot = necro_mach_add_member(program, &outer->machine_def, con_type, mach_symbol->ast).slot_num;
        return;
    }
}

void necro_core_transform_to_mach_2_app(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_APP);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);
    NecroCoreAst* function  = core_ast;
    size_t        arg_count = 0;
    while (function->ast_type == NECRO_CORE_AST_APP)
    {
        necro_core_transform_to_mach_2_go(program, function->app.expr2, outer);
        arg_count++;
        function = function->app.expr1;
    }
    assert(function->ast_type == NECRO_CORE_AST_VAR);
    NecroCoreAstSymbol* ast_symbol = function->var.ast_symbol;
    NecroMachAstSymbol* symbol     = ast_symbol->mach_symbol;
    assert(symbol != NULL);
    NecroMachAst*       fn_value   = symbol->ast;
    NecroMachType*      fn_type    = NULL;
    if (symbol->primop_type >  NECRO_PRIMOP_PRIM_FN)
        return;
    assert(fn_value != NULL);
    if (fn_value->type == NECRO_MACH_DEF)
    {
        fn_type = fn_value->machine_def.fn_type;
        if (fn_value->machine_def.num_members > 0)
        {
            // Double buffer deep copy state for recursive values
            if (fn_value->machine_def.symbol->is_deep_copy_fn && !outer->machine_def.symbol->is_deep_copy_fn)
            {
                NecroMachAst* const_init_value    = necro_mach_value_create_word_uint(program, 0);
                core_ast->persistent_slot         = necro_mach_add_member_full(program, &outer->machine_def, program->type_cache.word_uint_type, NULL, const_init_value).slot_num; // TODO: Initialize double_buffer_flag! How?
                NecroMachType* double_buffer_type = necro_mach_type_create_array(&program->arena, fn_value->necro_machine_type, 2);
                necro_mach_add_member(program, &outer->machine_def, double_buffer_type, NULL).slot_num;
            }
            else
            {
                // MachineDef State
                core_ast->persistent_slot = necro_mach_add_member(program, &outer->machine_def, fn_value->necro_machine_type, fn_value).slot_num;
            }
        }
    }
    else if (symbol->is_constructor)
    {
        assert(fn_value->necro_machine_type->type == NECRO_MACH_TYPE_FN);
        assert(fn_value->necro_machine_type->fn_type.num_parameters > 1);
        assert(fn_value->necro_machine_type->fn_type.parameters[0]->type == NECRO_MACH_TYPE_PTR);
        fn_type                 = fn_value->necro_machine_type;
        NecroMachType* con_type = fn_value->necro_machine_type->fn_type.parameters[0]->ptr_type.element_type;
        core_ast->persistent_slot = necro_mach_add_member(program, &outer->machine_def, con_type, fn_value).slot_num;
    }
    else
    {
        fn_type = fn_value->necro_machine_type;
    }
    assert(fn_type->type == NECRO_MACH_TYPE_FN);
    if (symbol->is_constructor)
        assert(fn_type->fn_type.num_parameters == (arg_count + 1));
    else
        assert(fn_type->fn_type.num_parameters == arg_count);
}

void necro_core_transform_to_mach_2_lit(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_LIT);
    assert(outer != NULL);
    assert(outer->type == NECRO_MACH_DEF);
    if (core_ast->lit.type != NECRO_AST_CONSTANT_ARRAY)
        return;
    NecroMachType* array_mach_type = necro_mach_type_from_necro_type(program, core_ast->necro_type);
    core_ast->persistent_slot  = necro_mach_add_member(program, &outer->machine_def, array_mach_type, NULL).slot_num;
    NecroCoreAstList* elements = core_ast->lit.array_literal_elements;
    while (elements != NULL)
    {
        necro_core_transform_to_mach_2_go(program, elements->data, outer);
        elements = elements->next;
    }
}

void necro_core_transform_to_mach_2_for(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_FOR);
    assert(outer != NULL);
    assert(outer->type == NECRO_MACH_DEF);

    //!!!!!!!!!!!!!
    // NOTE: Expects that the args are single arguments. If these are patterns in core then core needs to transform them to have a case statement added to the expression portion of the for loop!
    //!!!!!!!!!!!!!
    necro_core_transform_to_mach_2_go(program, core_ast->for_loop.range_init, outer);
    necro_core_transform_to_mach_2_go(program, core_ast->for_loop.value_init, outer);

    // Swap out old members
    size_t         old_members_size     = outer->machine_def.members_size;
    NecroMachSlot* old_members          = outer->machine_def.members;
    size_t         old_num_members      = outer->machine_def.num_members;
    outer->machine_def.members_size     = 4;
    outer->machine_def.members          = necro_paged_arena_alloc(&program->arena, outer->machine_def.members_size * sizeof(NecroMachSlot));
    outer->machine_def.num_members      = 0;

    necro_core_transform_to_mach_2_go(program, core_ast->for_loop.expression, outer);

    // Swap in old members
    NecroMachSlot* for_members          = outer->machine_def.members;
    size_t         for_num_members      = outer->machine_def.num_members;
    outer->machine_def.members_size     = old_members_size;
    outer->machine_def.members          = old_members;
    outer->machine_def.num_members      = old_num_members;

    //--------------------
    // Calculate For loop state type
    if (for_num_members == 0)
    {
        core_ast->persistent_slot = 0xFFFFFFFF;
        return;
    }
    NecroMachType**     for_member_types = necro_paged_arena_alloc(&program->arena, for_num_members * sizeof(NecroMachType*));
    for (size_t i = 0; i < for_num_members; ++i)
        for_member_types[i] = for_members[i].necro_machine_type;
    NecroMachAstSymbol* for_state_symbol = necro_mach_ast_symbol_gen(program, NULL, "LoopState", NECRO_MANGLE_NAME);
    NecroMachType*      for_state_type   = necro_mach_create_struct_def(program, for_state_symbol, for_member_types, for_num_members)->necro_machine_type;
    NecroMachType*      for_array_type   = necro_mach_type_create_array(&program->arena, for_state_type, core_ast->for_loop.max_loops);
    core_ast->persistent_slot            = necro_mach_add_member(program, &outer->machine_def, for_array_type, NULL).slot_num;
}

void necro_core_transform_to_mach_2_go(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    switch (core_ast->ast_type)
    {
    case NECRO_CORE_AST_VAR:       necro_core_transform_to_mach_2_var(program, core_ast, outer); return;
    case NECRO_CORE_AST_BIND:      necro_core_transform_to_mach_2_bind(program, core_ast, outer); return;
    case NECRO_CORE_AST_BIND_REC:  necro_core_transform_to_mach_2_bind_rec(program, core_ast, outer); return;
    case NECRO_CORE_AST_APP:       necro_core_transform_to_mach_2_app(program, core_ast, outer); return;
    case NECRO_CORE_AST_FOR:       necro_core_transform_to_mach_2_for(program, core_ast, outer); return;
    case NECRO_CORE_AST_LAM:       necro_core_transform_to_mach_2_lam(program, core_ast, outer); return;
    case NECRO_CORE_AST_LET:       necro_core_transform_to_mach_2_let(program, core_ast, outer); return;
    case NECRO_CORE_AST_CASE:      necro_core_transform_to_mach_2_case(program, core_ast, outer); return;
    case NECRO_CORE_AST_CASE_ALT:  assert(false && "found NECRO_CORE_AST_CASE_ALT in necro_core_transform_to_mach_2_go"); return;
    case NECRO_CORE_AST_LIT:       necro_core_transform_to_mach_2_lit(program, core_ast, outer); return;
    case NECRO_CORE_AST_DATA_DECL: return;
    case NECRO_CORE_AST_DATA_CON:  return;
    default:                       assert(false && "Unimplemented AST type in necro_core_transform_to_mach_2_go"); return;
    }
}

///////////////////////////////////////////////////////
// Pass 3
///////////////////////////////////////////////////////
NecroMachAst* necro_core_transform_to_mach_3_lit(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_LIT);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);
    switch (core_ast->lit.type)
    {
    case NECRO_AST_CONSTANT_INTEGER_PATTERN:
    case NECRO_AST_CONSTANT_INTEGER: return necro_mach_value_create_word_int(program, core_ast->lit.int_literal);
    case NECRO_AST_CONSTANT_FLOAT_PATTERN:
    case NECRO_AST_CONSTANT_FLOAT:   return necro_mach_value_create_word_float(program, core_ast->lit.float_literal);
    case NECRO_AST_CONSTANT_CHAR_PATTERN:
    case NECRO_AST_CONSTANT_CHAR:    return necro_mach_value_create_word_uint(program, core_ast->lit.char_literal);
    case NECRO_AST_CONSTANT_ARRAY:   /* CONTINUE BELOW */ break;
    default:                         assert(false); return NULL;
    }
    // NECRO_AST_CONSTANT_ARRAY
    NecroMachAst*     value_ptr = necro_mach_build_gep(program, outer->machine_def.update_fn, necro_mach_value_get_state_ptr(outer->machine_def.update_fn), (uint32_t[]) { 0, core_ast->persistent_slot }, 2, "state");
    NecroCoreAstList* elements  = core_ast->lit.array_literal_elements;
    size_t            i         = 0;
    while (elements != NULL)
    {
        NecroMachAst* element_value = necro_core_transform_to_mach_3_go(program, elements->data, outer);
        NecroMachAst* element_ptr   = necro_mach_build_gep(program, outer->machine_def.update_fn, value_ptr, (uint32_t[]) { 0, i }, 2, "elem");
        necro_mach_build_store(program, outer->machine_def.update_fn, element_value, element_ptr);
        elements                    = elements->next;
        i++;
    }
    return value_ptr;
}

NecroMachAst* necro_core_transform_to_mach_3_let(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_LET);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);
    necro_core_transform_to_mach_3_go(program, core_ast->let.bind, outer);
    return necro_core_transform_to_mach_3_go(program, core_ast->let.expr, outer);
}

NecroMachAst* necro_core_transform_to_mach_3_lam(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_LAM);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);
    // necro_core_transform_to_mach_3_go(program, core_ast->lambda.arg, outer);
    return necro_core_transform_to_mach_3_go(program, core_ast->lambda.expr, outer);
}

NecroMachAst* necro_core_transform_to_mach_3_var(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_VAR);
    assert(outer != NULL);
    assert(outer->type == NECRO_MACH_DEF);
    NecroCoreAstSymbol* ast_symbol = core_ast->var.ast_symbol;
    NecroMachAstSymbol* symbol     = ast_symbol->mach_symbol;
    // //--------------------
    // //  NULL
    // if (core_ast->var.id.id == program->null_con.id.id)
    // {
    //     return necro_create_null_necro_machine_value(program, program->necro_poly_ptr_type);
    // }
    //--------------------
    // Enum
    if (symbol->is_enum) // if (symbol->is_constructor && info->arity == 0 && info->is_enum) // NOTE: Only using the flag for now. Make sure this is accurate
    {
        return necro_mach_value_create_word_uint(program, symbol->con_num);
    }
    //--------------------
    // Primitive
    else if (symbol->is_primitive)
    {
        assert(symbol->ast != NULL);
        if (symbol->ast->type == NECRO_MACH_FN_DEF)
            return symbol->ast->fn_def.fn_value;
        else
            return symbol->ast;
    }
    //--------------------
    // Constructor
    else if (symbol->is_constructor)// && symbol->arity == 0) // NOTE: Not checking arity as theoretically that should already be caught be lambda lifting, etc
    {
        NecroMachAst* value_ptr = necro_mach_build_gep(program, outer->machine_def.update_fn, necro_mach_value_get_state_ptr(outer->machine_def.update_fn), (uint32_t[]) { 0, core_ast->persistent_slot }, 2, "state");
        return necro_mach_build_call(program, outer->machine_def.update_fn, symbol->ast, (NecroMachAst*[]) { value_ptr }, 1, NECRO_MACH_CALL_LANG, "con");
    }
    //--------------------
    // Global
    else if (symbol->ast->type == NECRO_MACH_VALUE && symbol->ast->value.value_type == NECRO_MACH_VALUE_GLOBAL)
    {
        return necro_mach_build_load(program, outer->machine_def.update_fn, symbol->ast, "glb");
    }
    //--------------------
    // Persistent value
    else if (symbol->ast->type == NECRO_MACH_DEF && symbol->ast->machine_def.state_type == NECRO_STATE_STATEFUL && symbol->ast->machine_def.outer != NULL)
    {
        // TODO: Cache result so that we don't keep reloading!
        assert(core_ast->var.ast_symbol->ast != NULL);
        const size_t persistent_slot = core_ast->var.ast_symbol->ast->persistent_slot;
        assert(persistent_slot != 0xFFFFFFFF);
        NecroMachAst* value_ptr = necro_mach_build_gep(program, outer->machine_def.update_fn, necro_mach_value_get_state_ptr(outer->machine_def.update_fn), (uint32_t[]) { 0, persistent_slot }, 2, "persistent_ptr");
        return necro_mach_build_load(program, outer->machine_def.update_fn, value_ptr, "persistent_value");
    }
    //--------------------
    // Local
    else if (symbol->ast->type == NECRO_MACH_VALUE)
    {
        return symbol->ast;
    }
    //--------------------
    // MACH_DEF global
    else if (symbol->ast->type == NECRO_MACH_DEF)
    {
        assert(symbol->ast->machine_def.global_value != NULL);
        return necro_mach_build_load(program, outer->machine_def.update_fn, symbol->ast->machine_def.global_value, "glb");
    }
    else
    {
        assert(false);
        return NULL;
    }
}

NecroMachAst* necro_core_transform_to_mach_3_primop(NecroMachProgram* program, NecroCoreAst* primop_var_ast, NecroCoreAst* app_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(primop_var_ast != NULL);
    assert(primop_var_ast->ast_type == NECRO_CORE_AST_VAR);
    assert(app_ast != NULL);
    assert(app_ast->ast_type == NECRO_CORE_AST_APP);
    assert(outer != NULL);
    assert(outer->type == NECRO_MACH_DEF);
    const NECRO_PRIMOP_TYPE primop_type = primop_var_ast->var.ast_symbol->primop_type;
    switch (primop_type)
    {
    case NECRO_PRIMOP_BINOP_IADD:
    case NECRO_PRIMOP_BINOP_ISUB:
    case NECRO_PRIMOP_BINOP_IMUL:
    case NECRO_PRIMOP_BINOP_IDIV:
    case NECRO_PRIMOP_BINOP_UADD:
    case NECRO_PRIMOP_BINOP_USUB:
    case NECRO_PRIMOP_BINOP_UMUL:
    case NECRO_PRIMOP_BINOP_UDIV:
    case NECRO_PRIMOP_BINOP_FADD:
    case NECRO_PRIMOP_BINOP_FSUB:
    case NECRO_PRIMOP_BINOP_FMUL:
    case NECRO_PRIMOP_BINOP_FDIV:
    case NECRO_PRIMOP_BINOP_AND:
    case NECRO_PRIMOP_BINOP_OR:
    case NECRO_PRIMOP_BINOP_SHL:
    case NECRO_PRIMOP_BINOP_SHR:
    {
        assert(app_ast->app.expr1->ast_type == NECRO_CORE_AST_APP);
        NecroMachAst* left  = necro_core_transform_to_mach_3_go(program, app_ast->app.expr1->app.expr2, outer);
        NecroMachAst* right = necro_core_transform_to_mach_3_go(program, app_ast->app.expr2, outer);
        return necro_mach_build_binop(program, outer->machine_def.update_fn, left, right, primop_type);
    }
    case NECRO_PRIMOP_UOP_IABS:
    case NECRO_PRIMOP_UOP_UABS:
    case NECRO_PRIMOP_UOP_FABS:
    case NECRO_PRIMOP_UOP_ISGN:
    case NECRO_PRIMOP_UOP_USGN:
    case NECRO_PRIMOP_UOP_FSGN:
    case NECRO_PRIMOP_UOP_ITOI:
    case NECRO_PRIMOP_UOP_ITOU:
    case NECRO_PRIMOP_UOP_ITOF:
    case NECRO_PRIMOP_UOP_UTOI:
    case NECRO_PRIMOP_UOP_FTRI:
    case NECRO_PRIMOP_UOP_FRNI:
    case NECRO_PRIMOP_UOP_FTOF:
    {
        NecroMachAst* param = necro_core_transform_to_mach_3_go(program, app_ast->app.expr2, outer);
        return necro_mach_build_uop(program, outer->machine_def.update_fn, param, primop_type);
    }
    case NECRO_PRIMOP_CMP_EQ:
    case NECRO_PRIMOP_CMP_NE:
    case NECRO_PRIMOP_CMP_GT:
    case NECRO_PRIMOP_CMP_GE:
    case NECRO_PRIMOP_CMP_LT:
    case NECRO_PRIMOP_CMP_LE:
    {
        assert(app_ast->app.expr1->ast_type == NECRO_CORE_AST_APP);
        NecroMachAst* left   = necro_core_transform_to_mach_3_go(program, app_ast->app.expr1->app.expr2, outer);
        NecroMachAst* right  = necro_core_transform_to_mach_3_go(program, app_ast->app.expr2, outer);
        NecroMachAst* result = necro_mach_build_cmp(program, outer->machine_def.update_fn, primop_type, left, right);
        return necro_mach_build_zext(program, outer->machine_def.update_fn, result, program->type_cache.word_uint_type);
    }
    default:
        assert(false);
        return NULL;
    }
}

NecroMachAst* necro_core_transform_to_mach_3_app(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_APP);
    assert(outer != NULL);
    assert(outer->type == NECRO_MACH_DEF);
    //--------------------
    // Count Args, Grab Fn var
    //--------------------
    NecroCoreAst* function        = core_ast;
    size_t        arg_count       = 0;
    size_t        persistent_slot = core_ast->persistent_slot;
    while (function->ast_type == NECRO_CORE_AST_APP)
    {
        arg_count++;
        function = function->app.expr1;
    }
    //--------------------
    // Get Fn value, establish state
    //--------------------
    assert(function->ast_type == NECRO_CORE_AST_VAR);
    NECRO_MACH_CALL_TYPE call_type  = NECRO_MACH_CALL_LANG;
    NecroCoreAstSymbol*  ast_symbol = function->var.ast_symbol;
    NecroMachAst*        fn_value   = ast_symbol->mach_symbol->ast;
    bool                 uses_state = false;
    if (ast_symbol->primop_type > NECRO_PRIMOP_PRIM_FN) //PRIM_OP Early Exit!
        return necro_core_transform_to_mach_3_primop(program, function, core_ast, outer);
    assert(fn_value != NULL);
    if (fn_value->type == NECRO_MACH_DEF)
    {
        // uses_state  = machine_def->machine_def.state_type == NECRO_STATE_STATEFUL;
        uses_state = fn_value->machine_def.num_members > 0;
        fn_value   = fn_value->machine_def.update_fn->fn_def.fn_value;
    }
    else if (ast_symbol->mach_symbol->is_constructor)
    {
        uses_state = true;
    }
    else if (fn_value->type == NECRO_MACH_FN_DEF)
    {
        call_type = (fn_value->fn_def.fn_type == NECRO_MACH_FN_RUNTIME) ? NECRO_MACH_CALL_C : NECRO_MACH_CALL_LANG;
        fn_value  = fn_value->fn_def.fn_value;
    }
    //--------------------
    // Go deeper on args
    //--------------------
    if (uses_state)
        arg_count++;
    assert(fn_value->type == NECRO_MACH_VALUE);
    assert(fn_value->necro_machine_type->type == NECRO_MACH_TYPE_FN);
    assert(fn_value->necro_machine_type->fn_type.num_parameters == arg_count);
    NecroArenaSnapshot snapshot  = necro_snapshot_arena_get(&program->snapshot_arena);
    NecroMachAst**     args      = necro_snapshot_arena_alloc(&program->snapshot_arena, arg_count * sizeof(NecroMachAst*));
    size_t             arg_index = arg_count - 1;
    function                     = core_ast;
    while (function->ast_type == NECRO_CORE_AST_APP)
    {
        args[arg_index] = necro_core_transform_to_mach_3_go(program, function->app.expr2, outer);
        function        = function->app.expr1;
        arg_index--;
    }
    //--------------------
    // Pass in state struct
    //--------------------
    if (uses_state)
    {
        NecroMachAst* state_ptr = necro_mach_value_get_state_ptr(outer->machine_def.update_fn);
        if (fn_value->machine_def.symbol->is_deep_copy_fn && !outer->machine_def.symbol->is_deep_copy_fn)
        {
            // Double Buffered deep_copy_fn state
            const size_t  double_buffer_slot      = persistent_slot + 1;
            NecroMachAst* double_buffer_flag_ptr  = necro_mach_build_gep(program, outer->machine_def.update_fn, state_ptr, (uint32_t[]) { 0, (uint32_t)persistent_slot }, 2, "double_buffer_flag_ptr");
            NecroMachAst* double_buffer_flag_val  = necro_mach_build_load(program, outer->machine_def.update_fn, double_buffer_flag_ptr, "double_buffer_flag_val");
            NecroMachAst* double_buffer_flag_not  = necro_mach_build_binop(program, outer->machine_def.update_fn, necro_mach_value_create_word_uint(program, 1), double_buffer_flag_val, NECRO_PRIMOP_BINOP_USUB);
            necro_mach_build_store(program, outer->machine_def.update_fn, double_buffer_flag_not, double_buffer_flag_ptr);
            args[0]                               = necro_mach_build_non_const_gep(program, outer->machine_def.update_fn, state_ptr, (NecroMachAst*[]) { necro_mach_value_create_word_uint(program, 0), necro_mach_value_create_word_uint(program, double_buffer_slot), double_buffer_flag_val }, 3, "state", fn_value->necro_machine_type->fn_type.parameters[0]);
        }
        else
        {
            // Normal State
            args[0] = necro_mach_build_gep(program, outer->machine_def.update_fn, state_ptr, (uint32_t[]) { 0, (uint32_t)persistent_slot }, 2, "state");
        }
    }
    //--------------------
    // Call fn
    //--------------------
    NecroMachAst* result = necro_mach_build_call(program, outer->machine_def.update_fn, fn_value, args, arg_count, call_type, "app");
    necro_snapshot_arena_rewind(&program->snapshot_arena, snapshot);
    return result;
}

void necro_core_transform_to_mach_3_initializer(NecroMachProgram* program, NecroCoreAst* bind_ast, NecroMachAst* outer)
{
    if (bind_ast->bind.initializer == NULL)
        return;
    assert(outer != NULL);
    assert(outer->machine_def.init_fn != NULL);
    necro_mach_value_put_state_ptr(outer->machine_def.init_fn, necro_mach_value_create_param_reg(program, outer->machine_def.init_fn, 0));
    NecroMachAst* update_fn         = outer->machine_def.update_fn;
    outer->machine_def.update_fn    = outer->machine_def.init_fn;
    // Go deeper
    NecroMachAst* initialized_value = necro_core_transform_to_mach_3_go(program, bind_ast->bind.initializer, outer);
    //--------------------
    // Gen code for initializing persistent value in init_fn
    if (outer != bind_ast->bind.ast_symbol->mach_symbol->ast)
    {
        // Local Binding, Store result in State Object
        NecroMachAst* data_ptr          = necro_mach_value_create_param_reg(program, outer->machine_def.init_fn, 0);
        NecroMachAst* initialized_ptr   = necro_mach_build_gep(program, outer->machine_def.init_fn, data_ptr, (uint32_t[]) { 0, (uint32_t)bind_ast->persistent_slot }, 2, "init");
        necro_mach_build_store(program, outer->machine_def.init_fn, initialized_value, initialized_ptr);
    }
    else
    {
        // Global Binding, Store result in global variable
        necro_mach_build_store(program, outer->machine_def.init_fn, initialized_value, outer->machine_def.global_value);
    }
    outer->machine_def.update_fn         = update_fn;
}

typedef enum { NECRO_MACH_IS_TOP, NECRO_MACH_IS_NOT_TOP } NECRO_MACH_TOP_TYPE;
NecroMachAst* necro_core_transform_to_mach_3_bind_expr(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer, NECRO_MACH_TOP_TYPE top_type)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_BIND);
    assert(outer != NULL);
    assert(outer->type == NECRO_MACH_DEF);

    NecroMachAst* current_state_ptr = necro_mach_value_get_state_ptr(outer->machine_def.update_fn);
    const size_t  persistent_slot   = core_ast->persistent_slot;
    const bool    is_recursive      = persistent_slot != 0xFFFFFFFF;
    NecroMachAst* result_value      = necro_core_transform_to_mach_3_go(program, core_ast->bind.expr, outer);

    if (is_recursive && top_type == NECRO_MACH_IS_NOT_TOP)
    {
        // Store Result
        const size_t  result_slot = core_ast->persistent_slot;
        NecroMachAst* result_ptr  = necro_mach_build_gep(program, outer->machine_def.update_fn, current_state_ptr, (uint32_t[]) { 0, result_slot }, 2, "result_ptr");
        necro_mach_build_store(program, outer->machine_def.update_fn, result_value, result_ptr);
    }
    return result_value;
}

NecroMachAst* necro_core_transform_to_mach_3_bind(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_BIND);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);

    if (core_ast->bind.ast_symbol->is_primitive)
        return NULL;

    //--------------------
    // Retrieve machine_def
    NecroMachAstSymbol* symbol      = core_ast->bind.ast_symbol->mach_symbol;
    NecroMachAst*       machine_def = symbol->ast;
    assert(machine_def != NULL);
    assert(machine_def->type == NECRO_MACH_DEF);

    //--------------------
    // Local bindings (early exit)
    if (outer != NULL)
    {
        necro_core_transform_to_mach_3_initializer(program, core_ast, outer);
        NecroMachAst* return_value   = necro_core_transform_to_mach_3_bind_expr(program, core_ast, outer, NECRO_MACH_IS_NOT_TOP);
        symbol->ast                  = return_value;
        return return_value;
    }
    necro_core_transform_to_mach_3_initializer(program, core_ast, machine_def);

    //--------------------
    // Global Bindings, create update type
    NecroArenaSnapshot  snapshot          = necro_snapshot_arena_get(&program->snapshot_arena);
    bool                uses_state        = machine_def->machine_def.num_members > 0;
    NecroMachAstSymbol* update_symbol     = necro_mach_ast_symbol_gen(program, NULL, necro_snapshot_arena_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "update", machine_def->machine_def.machine_name->name->str + 1 }), NECRO_MANGLE_NAME);
    update_symbol->is_deep_copy_fn        = machine_def->machine_def.symbol->is_deep_copy_fn;
    size_t              num_update_params = machine_def->machine_def.num_arg_names;
    if (num_update_params > 0)
        assert(machine_def->machine_def.num_arg_names == machine_def->machine_def.fn_type->fn_type.num_parameters);
    if (uses_state)
        num_update_params++;
    NecroMachType** update_params = necro_snapshot_arena_alloc(&program->snapshot_arena, num_update_params * sizeof(NecroMachType*));
    if (uses_state)
    {
        update_params[0] = necro_mach_type_create_ptr(&program->arena, machine_def->necro_machine_type);
    }
    for (size_t i = 0; i < machine_def->machine_def.num_arg_names; ++i)
    {
        update_params[i + (uses_state ? 1 : 0)] = machine_def->machine_def.fn_type->fn_type.parameters[i];
    }
    NecroMachType* update_fn_type = necro_mach_type_create_fn(&program->arena, necro_mach_type_make_ptr_if_boxed(program, machine_def->machine_def.value_type), update_params, num_update_params);
    NecroMachAst*  update_fn_body = necro_mach_block_create(program, "entry", NULL);
    NecroMachAst*  update_fn_def  = necro_mach_create_fn(program, update_symbol, update_fn_body, update_fn_type);
    program->functions.length--; // HACK: Don't want the update function in the functions list, instead it belongs to the machine
    machine_def->machine_def.update_fn = update_fn_def;
    for (size_t i = 0; i < machine_def->machine_def.num_arg_names; ++i)
    {
        machine_def->machine_def.arg_names[i]->ast = necro_mach_value_create_param_reg(program, update_fn_def, i + (uses_state ? 1 : 0));
    }

    //--------------------
    // Go deeper
    NecroMachAst* result = necro_core_transform_to_mach_3_bind_expr(program, core_ast, machine_def, NECRO_MACH_IS_TOP);

    //--------------------
    // Finish function
    necro_mach_build_return(program, update_fn_def, result);
    if (machine_def->machine_def.init_fn != NULL)
        necro_mach_build_return_void(program, machine_def->machine_def.init_fn);
    necro_snapshot_arena_rewind(&program->snapshot_arena, snapshot);
    return NULL;
}

NecroMachAst* necro_core_transform_to_mach_3_bind_rec(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_BIND_REC);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);
    assert(false && "TODO");
    return NULL;
}

NecroMachAst* necro_core_transform_to_mach_3_for(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_FOR);
    assert(outer != NULL);
    assert(outer->type == NECRO_MACH_DEF);

    //--------------------
    // Blocks
    NecroMachAst* current_block = necro_mach_block_get_current(outer->machine_def.update_fn);
    NecroMachAst* loop_block    = necro_mach_block_append(program, outer->machine_def.update_fn, "loop");

    //--------------------
    // Current Block
    necro_mach_block_move_to(program, outer->machine_def.update_fn, current_block);
    NecroMachAst*  range_value       = necro_core_transform_to_mach_3_go(program, core_ast->for_loop.range_init, outer);
    NecroMachAst*  init_value        = necro_core_transform_to_mach_3_go(program, core_ast->for_loop.value_init, outer);
    NecroMachAst*  init_index_ptr    = necro_mach_build_gep(program, outer->machine_def.update_fn, range_value, (uint32_t[]) { 0, 1 }, 2, "init_index_ptr");
    NecroMachAst*  init_index_value  = necro_mach_build_load(program, outer->machine_def.update_fn, init_index_ptr, "init_index");
    NecroMachAst*  increment_ptr     = necro_mach_build_gep(program, outer->machine_def.update_fn, range_value, (uint32_t[]) { 0, 2 }, 2, "increment_ptr");
    NecroMachAst*  increment_value   = necro_mach_build_load(program, outer->machine_def.update_fn, increment_ptr, "increment");
    NecroMachAst*  trim_ptr          = necro_mach_build_gep(program, outer->machine_def.update_fn, range_value, (uint32_t[]) { 0, 3 }, 2, "trim_ptr");
    NecroMachAst*  trim_value        = necro_mach_build_load(program, outer->machine_def.update_fn, trim_ptr, "trim");
    NecroMachAst*  max_index         = necro_mach_value_create_word_uint(program, core_ast->for_loop.max_loops);
    NecroMachAst*  end_index         = necro_mach_build_binop(program, outer->machine_def.update_fn, max_index, trim_value, NECRO_PRIMOP_BINOP_USUB);
    NecroMachAst*  current_state_ptr = necro_mach_value_get_state_ptr(outer->machine_def.update_fn);
    NecroMachAst*  for_state_array   = (core_ast->persistent_slot == 0xFFFFFFFF) ? NULL : necro_mach_build_gep(program, outer->machine_def.update_fn, current_state_ptr, (uint32_t[]) { 0, core_ast->persistent_slot }, 2, "for_state_array");
    NecroMachType* for_state_type    = (core_ast->persistent_slot == 0xFFFFFFFF) ? NULL : necro_mach_type_create_ptr(&program->arena, for_state_array->necro_machine_type->ptr_type.element_type->array_type.element_type);
    necro_mach_build_break(program, outer->machine_def.update_fn, loop_block);

    //--------------------
    // Loop Block
    necro_mach_block_move_to(program, outer->machine_def.update_fn, loop_block);
    NecroMachAst*  index_value                                     = necro_mach_build_phi(program, outer->machine_def.update_fn, program->type_cache.word_uint_type, NULL);
    NecroMachAst*  index_phi                                       = necro_mach_block_get_current(outer->machine_def.update_fn)->block.statements[necro_mach_block_get_current(outer->machine_def.update_fn)->block.num_statements - 1]; // HACK HACK HACK HACK
    NecroMachAst*  value_value                                     = necro_mach_build_phi(program, outer->machine_def.update_fn, init_value->necro_machine_type, NULL);
    NecroMachAst*  value_phi                                       = necro_mach_block_get_current(outer->machine_def.update_fn)->block.statements[necro_mach_block_get_current(outer->machine_def.update_fn)->block.num_statements - 1]; // HACK HACK HACK HACK
    if (core_ast->persistent_slot != 0xFFFFFFFF)
        necro_mach_value_put_state_ptr(outer->machine_def.update_fn, necro_mach_build_non_const_gep(program, outer->machine_def.update_fn, for_state_array, (NecroMachAst* []) { necro_mach_value_create_word_uint(program, 0), index_value }, 2, "loop_state", for_state_type));
    // Index
    core_ast->for_loop.index_arg->var.ast_symbol->mach_symbol->ast = index_value;
    necro_mach_add_incoming_to_phi(program, index_phi, current_block, init_index_value);
    // Value
    core_ast->for_loop.value_arg->var.ast_symbol->mach_symbol->ast = value_value;
    NecroMachAst* expression_value                                 = necro_core_transform_to_mach_3_go(program, core_ast->for_loop.expression, outer);
    NecroMachAst* curr_loop_block                                  = necro_mach_block_get_current(outer->machine_def.update_fn);
    necro_mach_add_incoming_to_phi(program, value_phi, current_block, init_value);
    necro_mach_add_incoming_to_phi(program, value_phi, curr_loop_block, expression_value);
    // Test and Jump
    NecroMachAst* loop_index                                       = necro_mach_build_binop(program, outer->machine_def.update_fn, index_value, increment_value, NECRO_PRIMOP_BINOP_UADD);
    NecroMachAst* is_less_than_value                               = necro_mach_build_cmp(program, outer->machine_def.update_fn, NECRO_PRIMOP_CMP_LT, loop_index, end_index);
    necro_mach_add_incoming_to_phi(program, index_phi, curr_loop_block, loop_index);
    NecroMachAst* next_block                                       = necro_mach_block_append(program, outer->machine_def.update_fn, "next");
    necro_mach_build_cond_break(program, outer->machine_def.update_fn, is_less_than_value, loop_block, next_block);

    //--------------------
    // Next Block
    necro_mach_block_move_to(program, outer->machine_def.update_fn, next_block);
    necro_mach_value_put_state_ptr(outer->machine_def.update_fn, current_state_ptr);
    return expression_value;
}

NecroMachAst* necro_core_transform_to_mach_3_go(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    switch (core_ast->ast_type)
    {
    case NECRO_CORE_AST_LIT:       return necro_core_transform_to_mach_3_lit(program, core_ast, outer);
    case NECRO_CORE_AST_LET:       return necro_core_transform_to_mach_3_let(program, core_ast, outer);
    case NECRO_CORE_AST_LAM:       return necro_core_transform_to_mach_3_lam(program, core_ast, outer);
    case NECRO_CORE_AST_FOR:       return necro_core_transform_to_mach_3_for(program, core_ast, outer);
    case NECRO_CORE_AST_CASE:      return necro_core_transform_to_mach_3_case(program, core_ast, outer);
    case NECRO_CORE_AST_VAR:       return necro_core_transform_to_mach_3_var(program, core_ast, outer);
    case NECRO_CORE_AST_APP:       return necro_core_transform_to_mach_3_app(program, core_ast, outer);
    case NECRO_CORE_AST_BIND:      return necro_core_transform_to_mach_3_bind(program, core_ast, outer);
    case NECRO_CORE_AST_BIND_REC:  return necro_core_transform_to_mach_3_bind_rec(program, core_ast, outer);
    case NECRO_CORE_AST_DATA_DECL: return NULL;
    case NECRO_CORE_AST_DATA_CON:  return NULL;
    default:                       assert(false && "Unimplemented AST type in necro_core_transform_to_mach_3_go"); return NULL;
    }
}


///////////////////////////////////////////////////////
// Construct Main
///////////////////////////////////////////////////////
void necro_mach_construct_main(NecroMachProgram* program)
{
    //--------------------
    // Get Program Main
    NecroAstSymbol* main_symbol = necro_symtable_get_top_level_ast_symbol(program->base->scoped_symtable, necro_intern_string(program->intern, "main"));
    if (main_symbol != NULL)
    {
        // TODO: Handle no main in program
        assert(main_symbol->core_ast_symbol != NULL);
        program->program_main = main_symbol->core_ast_symbol->mach_symbol->ast;
    }

    //--------------------
    // Declare NecroMain
    NecroMachAstSymbol* necro_main_symbol = necro_mach_ast_symbol_gen(program, NULL, "necro_main", NECRO_DONT_MANGLE);
    NecroMachType*      necro_main_type   = necro_mach_type_create_fn(&program->arena, necro_mach_type_create_void(program), NULL, 0);
    NecroMachAst*       necro_main_entry  = necro_mach_block_create(program, "entry", NULL);
    NecroMachAst*       necro_main_fn     = necro_mach_create_fn(program, necro_main_symbol, necro_main_entry, necro_main_type);
    program->functions.length--; // Hack...
    NecroMachAst*       necro_main_loop   = necro_mach_block_append(program, necro_main_fn, "loop");
    NecroMachAst*       necro_main_done   = necro_mach_block_append(program, necro_main_fn, "done");

    //--------------------
    // entry
    necro_mach_build_break(program, necro_main_fn, necro_main_loop);
    necro_mach_build_call(program, necro_main_fn, program->runtime.necro_init_runtime->ast->fn_def.fn_value, NULL, 0, NECRO_MACH_CALL_C, "");

    //--------------------
    // Create states
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        if (program->machine_defs.data[i]->machine_def.num_members == 0 || program->machine_defs.data[i]->machine_def.num_arg_names != 0)
            continue;
        program->machine_defs.data[i]->machine_def.global_state = necro_mach_build_call(program, necro_main_fn, program->machine_defs.data[i]->machine_def.mk_fn->fn_def.fn_value, NULL, 0, NECRO_MACH_CALL_LANG, "state");
    }
    if (program->program_main != NULL && program->program_main->machine_def.num_members > 0)
    {
        program->program_main->machine_def.global_state = necro_mach_build_call(program, necro_main_fn, program->program_main->machine_def.mk_fn->fn_def.fn_value, NULL, 0, NECRO_MACH_CALL_LANG, "main_state");
    }

    //--------------------
    // Call constants
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        if (program->machine_defs.data[i]->machine_def.state_type > NECRO_STATE_CONSTANT || program->machine_defs.data[i]->machine_def.num_arg_names != 0)
            continue;
        if (program->machine_defs.data[i]->machine_def.num_members > 0)
        {
            NecroMachAst* state  = program->machine_defs.data[i]->machine_def.global_state;
            NecroMachAst* result = necro_mach_build_call(program, necro_main_fn, program->machine_defs.data[i]->machine_def.update_fn->fn_def.fn_value, (NecroMachAst*[]) { state }, 1, NECRO_MACH_CALL_LANG, "constant_result");
            necro_mach_build_store(program, necro_main_fn, result, program->machine_defs.data[i]->machine_def.global_value);
        }
        else
        {
            NecroMachAst* result = necro_mach_build_call(program, necro_main_fn, program->machine_defs.data[i]->machine_def.update_fn->fn_def.fn_value, NULL, 0, NECRO_MACH_CALL_LANG, "constant_result");
            necro_mach_build_store(program, necro_main_fn, result, program->machine_defs.data[i]->machine_def.global_value);
        }
    }

    //--------------------
    // loop
    necro_mach_block_move_to(program, necro_main_fn, necro_main_loop);
    necro_mach_build_call(program, necro_main_fn, program->runtime.necro_update_runtime->ast->fn_def.fn_value, NULL, 0, NECRO_MACH_CALL_C, "");
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        if (program->machine_defs.data[i]->machine_def.state_type == NECRO_STATE_CONSTANT || program->machine_defs.data[i]->machine_def.num_arg_names != 0)
            continue;
        if (program->machine_defs.data[i]->machine_def.num_members > 0)
        {
            NecroMachAst* state  = program->machine_defs.data[i]->machine_def.global_state;
            NecroMachAst* result = necro_mach_build_call(program, necro_main_fn, program->machine_defs.data[i]->machine_def.update_fn->fn_def.fn_value, (NecroMachAst*[]) { state }, 1, NECRO_MACH_CALL_LANG, "stateful_result");
            necro_mach_build_store(program, necro_main_fn, result, program->machine_defs.data[i]->machine_def.global_value);
        }
        else
        {
            NecroMachAst* result = necro_mach_build_call(program, necro_main_fn, program->machine_defs.data[i]->machine_def.update_fn->fn_def.fn_value, NULL, 0, NECRO_MACH_CALL_LANG, "pointwise_result");
            necro_mach_build_store(program, necro_main_fn, result, program->machine_defs.data[i]->machine_def.global_value);
        }
    }

    //--------------------
    // Call Main
    if (program->program_main != NULL)
    {
        // NOTE: Main needs to be of type World -> World, which translates to fn main(u32) -> u32
        NecroMachAst* world_value = necro_mach_value_create_word_uint(program, 0);
        if (program->program_main->machine_def.num_members > 0)
        {
            NecroMachAst* state  = program->program_main->machine_def.global_state;
            NecroMachAst* result = necro_mach_build_call(program, necro_main_fn, program->program_main->machine_def.update_fn->fn_def.fn_value, (NecroMachAst*[]) { state, world_value }, 2, NECRO_MACH_CALL_LANG, "main_result");
            UNUSED(result);
        }
        else
        {
            NecroMachAst* result = necro_mach_build_call(program, necro_main_fn, program->program_main->machine_def.update_fn->fn_def.fn_value, (NecroMachAst*[]) { world_value }, 1, NECRO_MACH_CALL_LANG, "main_result");
            UNUSED(result);
        }
    }

    //--------------------
    // Loop Or Finish
    necro_mach_build_call(program, necro_main_fn, program->runtime.necro_sleep->ast->fn_def.fn_value, (NecroMachAst*[]) { necro_mach_value_create_uint32(program, 10) }, 1, NECRO_MACH_CALL_C, "");
    NecroMachAst* is_done     = necro_mach_build_call(program, necro_main_fn, program->runtime.necro_runtime_is_done->ast->fn_def.fn_value, NULL, 0, NECRO_MACH_CALL_C, "is_done");
    NecroMachAst* is_done_cmp = necro_mach_build_cmp(program, necro_main_fn, NECRO_PRIMOP_CMP_GT, is_done, necro_mach_value_create_word_uint(program, 0));
    necro_mach_build_cond_break(program, necro_main_fn, is_done_cmp, necro_main_done, necro_main_loop);
    necro_mach_block_move_to(program, necro_main_fn, necro_main_done);

    //--------------------
    // Clean up
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        if (program->machine_defs.data[i]->machine_def.global_state == NULL)
            continue;
        // Destroy state
        NecroMachAst* data_ptr = necro_mach_build_bit_cast(program, necro_main_fn, program->machine_defs.data[i]->machine_def.global_state, necro_mach_type_create_ptr(&program->arena, program->type_cache.uint8_type));
        necro_mach_build_call(program, necro_main_fn, program->runtime.necro_runtime_free->ast->fn_def.fn_value, (NecroMachAst* []) { data_ptr }, 1, NECRO_MACH_CALL_C, "");
    }
    // if (program->program_main->machine_def.global_state != NULL)
    // {
    //     // Destroy state
    //     NecroMachAst* data_ptr = necro_mach_build_bit_cast(program, necro_main_fn, program->program_main->machine_def.global_state, necro_mach_type_create_ptr(&program->arena, program->type_cache.uint8_type));
    //     necro_mach_build_call(program, necro_main_fn, program->runtime.necro_runtime_free->ast->fn_def.fn_value, (NecroMachAst* []) { data_ptr }, 1, NECRO_MACH_CALL_C, "");
    // }
    necro_mach_build_return_void(program, necro_main_fn);
    program->necro_main = necro_main_fn;

}

///////////////////////////////////////////////////////
// Transform Core to Mach
///////////////////////////////////////////////////////
void necro_core_transform_to_mach(NecroCompileInfo info, NecroIntern* intern, NecroBase* base, NecroCoreAstArena* core_ast_arena, NecroMachProgram* program)
{
    *program = necro_mach_program_create(intern, base);

    //---------------
    // Pass 1
    NecroCoreAst* top = core_ast_arena->root;
    while (top != NULL && top->ast_type == NECRO_CORE_AST_LET)
    {
        necro_core_transform_to_mach_1_go(program, top->let.bind, NULL);
        top = top->let.expr;
    }
    if (top != NULL)
        necro_core_transform_to_mach_1_go(program, top, NULL);

    //---------------
    // Pass 2
    top = core_ast_arena->root;
    while (top != NULL && top->ast_type == NECRO_CORE_AST_LET)
    {
        necro_core_transform_to_mach_2_go(program, top->let.bind, NULL);
        top = top->let.expr;
    }
    if (top != NULL)
        necro_core_transform_to_mach_2_go(program, top, NULL);

    //---------------
    // Pass 3
    top = core_ast_arena->root;
    while (top != NULL && top->ast_type == NECRO_CORE_AST_LET)
    {
        necro_core_transform_to_mach_3_go(program, top->let.bind, NULL);
        top = top->let.expr;
    }
    if (top != NULL)
        necro_core_transform_to_mach_3_go(program, top, NULL);

    //---------------
    // Construct main
    necro_mach_construct_main(program);

    //---------------
    // Verify and Print
    necro_mach_program_verify(program);
    if (info.compilation_phase == NECRO_PHASE_TRANSFORM_TO_MACHINE && info.verbosity > 0)
        necro_mach_print_program(program);
}


///////////////////////////////////////////////////////
// Testing
///////////////////////////////////////////////////////
#define NECRO_MACH_TEST_VERBOSE 1
void necro_mach_test_string(const char* test_name, const char* str)
{

    //--------------------
    // Set up
    NecroIntern         intern          = necro_intern_create();
    NecroSymTable       symtable        = necro_symtable_create(&intern);
    NecroScopedSymTable scoped_symtable = necro_scoped_symtable_create(&symtable);
    NecroBase           base            = necro_base_compile(&intern, &scoped_symtable);

    NecroLexTokenVector tokens          = necro_empty_lex_token_vector();
    NecroParseAstArena  parse_ast       = necro_parse_ast_arena_empty();
    NecroAstArena       ast             = necro_ast_arena_empty();
    NecroCoreAstArena   core_ast        = necro_core_ast_arena_empty();
    NecroMachProgram    mach_program    = necro_mach_program_empty();
    NecroCompileInfo    info            = necro_test_compile_info();
    info.verbosity = 2;

    //--------------------
    // Compile
    unwrap(void, necro_lex(info, &intern, str, strlen(str), &tokens));
    unwrap(void, necro_parse(info, &intern, &tokens, necro_intern_string(&intern, "Test"), &parse_ast));
    ast = necro_reify(info, &intern, &parse_ast);
    necro_build_scopes(info, &scoped_symtable, &ast);
    unwrap(void, necro_rename(info, &scoped_symtable, &intern, &ast));
    necro_dependency_analyze(info, &intern, &ast);
    necro_alias_analysis(info, &ast); // NOTE: Consider merging alias_analysis into RENAME_VAR phase?
    unwrap(void, necro_infer(info, &intern, &scoped_symtable, &base, &ast));
    unwrap(void, necro_monomorphize(info, &intern, &scoped_symtable, &base, &ast));
    unwrap(void, necro_ast_transform_to_core(info, &intern, &base, &ast, &core_ast));
    unwrap(void, necro_core_infer(&intern, &base, &core_ast));
    necro_core_lambda_lift(info, &intern, &base, &core_ast);
    necro_core_defunctionalize(info, &intern, &base, &core_ast);
    necro_core_state_analysis(info, &intern, &base, &core_ast);
    necro_core_transform_to_mach(info, &intern, &base, &core_ast, &mach_program);

    //--------------------
    // Print
#if NECRO_MACH_TEST_VERBOSE
    // printf("\n");
    // necro_core_ast_pretty_print(core_ast.root);
    printf("\n");
    necro_mach_print_program(&mach_program);
#endif
    printf("NecroMach %s test: Passed\n", test_name);
    fflush(stdout);

    //--------------------
    // Clean up
    necro_mach_program_destroy(&mach_program);
    necro_core_ast_arena_destroy(&core_ast);
    necro_ast_arena_destroy(&ast);
    necro_base_destroy(&base);
    necro_parse_ast_arena_destroy(&parse_ast);
    necro_destroy_lex_token_vector(&tokens);
    necro_scoped_symtable_destroy(&scoped_symtable);
    necro_symtable_destroy(&symtable);
    necro_intern_destroy(&intern);
}


void necro_mach_test()
{
    necro_announce_phase("Mach");

/*

    {
        const char* test_name   = "Basic 0";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Basic 1";
        const char* test_source = ""
            "x = True\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Data 1";
        const char* test_source = ""
            "data TwoInts = TwoInts Int Int\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Data 2";
        const char* test_source = ""
            "data TwoInts  = TwoInts Int Int\n"
            "data DoubleUp = DoubleUp TwoInts TwoInts\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Data 3";
        const char* test_source = ""
            "data TwoInts   = TwoInts Int Int\n"
            "data DoubleUp  = DoubleUp TwoInts TwoInts\n"
            "data TwoOrFour = Two TwoInts | Four DoubleUp DoubleUp\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Bind 1";
        const char* test_source = ""
            "data TwoInts = TwoInts Int Int\n"
            "twoForOne :: Int -> TwoInts\n"
            "twoForOne i = TwoInts i i\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Bind 2";
        const char* test_source = ""
            "data TwoInts   = TwoInts Int Int\n"
            "data DoubleUp  = DoubleUp TwoInts TwoInts\n"
            "doubleDown :: Int -> DoubleUp\n"
            "doubleDown i = DoubleUp (TwoInts i i) (TwoInts i i)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Bind 3";
        const char* test_source = ""
            "data TwoOrNone = None | TwoInts Int Int \n"
            "nope :: Int -> TwoOrNone\n"
            "nope i = None\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Main 1";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = printInt 0 w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Main 2";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = printInt mouseY (printInt mouseX w)\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Main 3";
        const char* test_source = ""
            "data TwoInts  = TwoInts Int Int\n"
            "data DoubleUp = DoubleUp TwoInts TwoInts\n"
            "doubleDown :: Int -> DoubleUp\n"
            "doubleDown i = DoubleUp (TwoInts i i) (TwoInts i i)\n"
            "rune :: DoubleUp\n"
            "rune = doubleDown mouseX\n"
            "main :: *World -> *World\n"
            "main w = printInt 0 w where\n"
            "  x = doubleDown 666\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 2";
        const char* test_source = ""
            "data SomeOrNone = Some Int | None\n"
            "main w =\n"
            "  case None of\n"
            "    None -> printInt 0 w\n"
            "    _    -> printInt 1 w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 3";
        const char* test_source = ""
            "data SomeOrNone = Some Int | None\n"
            "main w =\n"
            "  case None of\n"
            "    None   -> printInt 0 w\n"
            "    Some _ -> printInt 1 w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 4";
        const char* test_source = ""
            "data SomeOrNone = Some Int | None\n"
            "main w =\n"
            "  case Some 666 of\n"
            "    None   -> printInt 0 w\n"
            "    Some i -> printInt i w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 5";
        const char* test_source = ""
            "data TwoOfAKind = TwoOfAKind Int Int\n"
            "data SomeOrNone = Some TwoOfAKind | None\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case Some (TwoOfAKind 6 7) of\n"
            "    None -> w\n"
            "    Some (TwoOfAKind i1 i2) -> printInt i2 (printInt i1 w)\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 6";
        const char* test_source = ""
            "data TwoOfAKind = TwoOfAKind Int Int\n"
            "main w =\n"
            "  case TwoOfAKind 6 7 of\n"
            "    TwoOfAKind i1 i2 -> printInt i2 (printInt i1 w)\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 7";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w =\n"
            "  case 0 of\n"
            "    i -> printInt i w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 8";
        const char* test_source = ""
            "data Inner = SomeInner Int | NoInner\n"
            "data Outer = SomeOuter Inner | NoOuter\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case SomeOuter (SomeInner 6) of\n"
            "    NoOuter -> w\n"
            "    SomeOuter NoInner -> w\n"
            "    SomeOuter (SomeInner i) -> printInt i w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 9";
        const char* test_source = ""
            "data TwoForOne = One Int | Two Int Int\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case One 666 of\n"
            "    One x -> printInt x w\n"
            "    Two y z -> printInt z (printInt y w)\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 10";
        const char* test_source = ""
            "data TwoForOne = One Int | Two Int Int\n"
            "data MaybeTwoForOne = JustTwoForOne TwoForOne | NoneForOne\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case NoneForOne of\n"
            "    NoneForOne -> w\n"
            "    JustTwoForOne (One x) -> printInt x w\n"
            "    JustTwoForOne (Two y z) -> printInt z (printInt y w)\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 11";
        const char* test_source = ""
            "data OneOrZero    = One Int | Zero\n"
            "data MoreThanZero = MoreThanZero OneOrZero OneOrZero\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case MoreThanZero (One 0) Zero of\n"
            "    MoreThanZero (One i1)  (One i2) -> printInt i2 (printInt i1 w)\n"
            "    MoreThanZero Zero      (One i3) -> printInt i3 w\n"
            "    MoreThanZero (One i4)  Zero     -> printInt i4 w\n"
            "    MoreThanZero Zero      Zero     -> w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 12";
        const char* test_source = ""
            "data TwoForOne  = One Int | Two Int Int\n"
            "data FourForOne = MoreThanZero TwoForOne TwoForOne | Zero\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case Zero of\n"
            "    Zero -> w\n"
            "    MoreThanZero (One i1)     (One i2)      -> printInt i2 (printInt i1 w)\n"
            "    MoreThanZero (Two i3 i4)  (One i5)      -> printInt i5 (printInt i4 (printInt i3 w))\n"
            "    MoreThanZero (One i6)     (Two i7  i8)  -> printInt i8 (printInt i7 (printInt i6 w))\n"
            "    MoreThanZero (Two i9 i10) (Two i11 i12) -> printInt i12 (printInt i11 (printInt i10 (printInt i9 w)))\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 13";
        const char* test_source = ""
            "data OneOrZero    = One Int | Zero\n"
            "data MoreThanZero = MoreThanZero OneOrZero OneOrZero\n"
            "oneUp :: MoreThanZero -> MoreThanZero\n"
            "oneUp m =\n"
            "  case m of\n"
            "    MoreThanZero (One i1)  (One i2) -> m\n"
            "    MoreThanZero Zero      (One i3) -> MoreThanZero (One i3) (One i3)\n"
            "    MoreThanZero (One i4)  Zero     -> MoreThanZero (One i4) (One i4)\n"
            "    MoreThanZero Zero      Zero     -> m\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Poly 1";
        const char* test_source = ""
            "nothingFromSomething :: Maybe Int -> Maybe Int\n"
            "nothingFromSomething m = m\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Poly 2";
        const char* test_source = ""
            "nothingFromSomething :: Maybe (Maybe Int) -> Maybe (Maybe Int)\n"
            "nothingFromSomething m = m\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Poly 3";
        const char* test_source = ""
            "nothingFromSomething :: (Maybe Float, Maybe Int) -> (Maybe Float, Maybe Int)\n"
            "nothingFromSomething m = m\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Poly 4";
        const char* test_source = ""
            "nothingFromSomething :: Int -> Maybe Int\n"
            "nothingFromSomething i = Just i\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Poly 5";
        const char* test_source = ""
            "nothingFromSomething :: Int -> (Maybe Float, Maybe Int)\n"
            "nothingFromSomething i = (Nothing, Just i)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Poly Case 1";
        const char* test_source = ""
            "somethingFromNothing :: Maybe Int -> Int\n"
            "somethingFromNothing m =\n"
            "  case m of\n"
            "    Nothing -> 0\n"
            "    Just i  -> i\n"
            "main :: *World -> *World\n"
            "main w = printInt (somethingFromNothing Nothing) w\n";
        necro_mach_test_string(test_name, test_source);
    }


    {
        const char* test_name   = "Poly Case 2";
        const char* test_source = ""
            "somethingFromNothing :: Maybe Int -> *World -> *World\n"
            "somethingFromNothing m w =\n"
            "  case m of\n"
            "    Nothing -> printInt 0 w\n"
            "    Just i  -> printInt i w\n"
            "main :: *World -> *World\n"
            "main w = somethingFromNothing Nothing w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Poly Case 3";
        const char* test_source = ""
            "somethingFromNothing :: Maybe Int -> (Int, Float)\n"
            "somethingFromNothing m =\n"
            "  case m of\n"
            "    Nothing -> (0, 0)\n"
            "    Just i  -> (i, fromInt i)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 1";
        const char* test_source = ""
            "main w =\n"
            "  case 0 of\n"
            "    0 -> printInt 0 w\n"
            "    1 -> printInt 1 w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 2";
        const char* test_source = ""
            "main w =\n"
            "  case False of\n"
            "    False -> printInt 0 w\n"
            "    True  -> printInt 1 w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 3";
        const char* test_source = ""
            "basketCase :: Int -> *World -> *World\n"
            "basketCase i w =\n"
            "  case i of\n"
            "    0 -> printInt 0 w\n"
            "    1 -> printInt 1 w\n"
            "main :: *World -> *World\n"
            "main w = basketCase 0 w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 4";
        const char* test_source = ""
            "basketCase :: Int -> *World -> *World\n"
            "basketCase i w =\n"
            "  case i of\n"
            "    0 -> printInt 666 w\n"
            "    u -> printInt u w\n"
            "main :: *World -> *World\n"
            "main w = basketCase 0 w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 5";
        const char* test_source = ""
            "basketCase :: Int -> *World -> *World\n"
            "basketCase i w =\n"
            "  case i of\n"
            "    u -> printInt u w\n"
            "main :: *World -> *World\n"
            "main w = basketCase 0 w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 5";
        const char* test_source = ""
            "basketCase :: Maybe Int -> *World -> *World\n"
            "basketCase m w =\n"
            "  case m of\n"
            "    Nothing -> printInt 100 w\n"
            "    Just 0  -> printInt 200 w\n"
            "    Just -1 -> printInt 300 w\n"
            "    Just i  -> printInt i w\n"
            "main :: *World -> *World\n"
            "main w = basketCase Nothing w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 6";
        const char* test_source = ""
            "basketCase :: Maybe Int -> *World -> *World\n"
            "basketCase m w =\n"
            "  case m of\n"
            "    Nothing -> printInt 100 w\n"
            "    Just 0  -> printInt 200 w\n"
            "    Just -1 -> printInt 300 w\n"
            "    Just _  -> printInt 400 w\n"
            "main :: *World -> *World\n"
            "main w = basketCase Nothing w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 7";
        const char* test_source = ""
            "basketCase :: Int -> *World -> *World\n"
            "basketCase i w =\n"
            "  case gt i 0 of\n"
            "    True -> printInt 666 w\n"
            "    _    -> printInt 777 w\n"
            "main :: *World -> *World\n"
            "main w = basketCase 0 w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 8";
        const char* test_source = ""
            "basketCase :: Int -> *World -> *World\n"
            "basketCase i w =\n"
            "  case gt i 0 of\n"
            "    True  -> printInt 666 w\n"
            "    False -> printInt 777 w\n"
            "main :: *World -> *World\n"
            "main w = basketCase 0 w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 9";
        const char* test_source = ""
            "basketCase :: Int -> *World -> *World\n"
            "basketCase i w =\n"
            "  case Just (gt i 0) of\n"
            "    Nothing    -> printInt 0 w\n"
            "    Just True  -> printInt 666 w\n"
            "    Just False -> printInt 777 w\n"
            "main :: *World -> *World\n"
            "main w = basketCase 0 w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 10";
        const char* test_source = ""
            "basketCase :: Int -> *World -> *World\n"
            "basketCase i w =\n"
            "  case Just (gt i 0) of\n"
            "    Nothing    -> printInt 0 w\n"
            "    Just False -> printInt 777 w\n"
            "    Just _     -> printInt 666 w\n"
            "main :: *World -> *World\n"
            "main w = basketCase 0 w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Array 1";
        const char* test_source = ""
            "arrayed :: Array 2 Float -> Array 2 Float\n"
            "arrayed x = x\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Array 2";
        const char* test_source = ""
            "arrayed :: Maybe (Array 2 Float) -> Maybe (Array 2 Float)\n"
            "arrayed x = x\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Array 2.5";
        const char* test_source = ""
            "arrayed :: Array 2 Float\n"
            "arrayed = { 0, 1 }\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Array 3";
        const char* test_source = ""
            "arrayed :: Array 2 (Maybe Float)\n"
            "arrayed = { Nothing, Just 1 }\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Array 4";
        const char* test_source = ""
            "justMaybe :: Float -> Maybe Float\n"
            "justMaybe f = Just f\n"
            "arrayed :: Array 2 (Maybe Float)\n"
            "arrayed = { justMaybe 666, Just 1 }\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Index 1";
        const char* test_source = ""
            "iiCaptain :: Index 666 -> Index 666\n"
            "iiCaptain i = i\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "For Loop 1";
        const char* test_source = ""
            "tenTimes :: Range 10\n"
            "tenTimes = each\n"
            "loopTenTimes :: Int\n"
            "loopTenTimes = for tenTimes 0 loop i x -> mul x 2\n"
            "main :: *World -> *World\n"
            "main w = printInt loopTenTimes w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "For Loop 2";
        const char* test_source = ""
            "tenTimes :: Range 10\n"
            "tenTimes = each\n"
            "loopTenTimes :: Int\n"
            "loopTenTimes =\n"
            "  for tenTimes 0 loop i1 x1 ->\n"
            "    for tenTimes x1 loop i2 x2 ->\n"
            "      add x1 1\n"
            "main :: *World -> *World\n"
            "main w = printInt loopTenTimes w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "For Loop 3";
        const char* test_source = ""
            "tenTimes :: Range 10\n"
            "tenTimes = each\n"
            "loopTenTimes :: (Int, Int)\n"
            "loopTenTimes = (for tenTimes 0 loop i x -> mul x 2, for tenTimes 0 loop i x -> add x 2)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "For Loop 4";
        const char* test_source = ""
            "tenTimes :: Range 10\n"
            "tenTimes = each\n"
            "loopTenTimes :: Int\n"
            "loopTenTimes =\n"
            "  case Nothing of\n"
            "    Nothing -> for tenTimes 0 loop i x -> add x 1\n"
            "    Just y  -> for tenTimes 0 loop i x -> add x y\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "For Loop 5";
        const char* test_source = ""
            "tenTimes :: Range 10\n"
            "tenTimes = each\n"
            "loopTenTimes :: Maybe UInt\n"
            "loopTenTimes =\n"
            "  for tenTimes Nothing loop i x ->\n"
            "    case x of\n"
            "      Nothing -> Just 0\n"
            "      Just x  -> Just (add x 1)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "For Loop 6";
        const char* test_source = ""
            "tenTimes :: Range 10\n"
            "tenTimes = each\n"
            "loopTenTimes :: Maybe UInt\n"
            "loopTenTimes =\n"
            "  for tenTimes Nothing loop i x ->\n"
            "    Just 0\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "For Loop 3.5";
        const char* test_source = ""
            "tenTimes :: Range 10\n"
            "tenTimes = each\n"
            "loopTenTimes :: (Maybe Int, Maybe Int)\n"
            "loopTenTimes = (for tenTimes Nothing loop i x -> Just 0, for tenTimes Nothing loop i y -> Nothing)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "For Loop 2.5";
        const char* test_source = ""
            "tenTimes :: Range 10\n"
            "tenTimes = each\n"
            "twentyTimes :: Range 20\n"
            "twentyTimes = each\n"
            "loopTenTimes :: Maybe (Int, Int)\n"
            "loopTenTimes =\n"
            "  for tenTimes Nothing loop i1 x1 ->\n"
            "    for twentyTimes x1 loop i2 x2 ->\n"
            "      Just (1, 666)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Deep Copy Test 1";
        const char* test_source = ""
            "maybeZero :: Maybe Int -> Int\n"
            "maybeZero m = 0\n"
            "main :: *World -> *World\n"
            "main w = printInt (maybeZero Nothing) w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Deep Copy Test 2";
        const char* test_source = ""
            "maybeZero :: (Bool, Int, Float) -> Int\n"
            "maybeZero m = 0\n"
            "main :: *World -> *World\n"
            "main w = printInt (maybeZero (True, 0, 1)) w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Deep Copy Test 3";
        const char* test_source = ""
            "maybeZero :: Maybe (Maybe Bool, Int, (Float, Float)) -> Int\n"
            "maybeZero m = 0\n"
            "main :: *World -> *World\n"
            "main w = printInt (maybeZero Nothing) w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Deep Copy Test 3";
        const char* test_source = ""
            "maybeZero :: ((Int, Int), (Float, Float)) -> Int\n"
            "maybeZero m = 0\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 1";
        const char* test_source = ""
            "counter :: Int\n"
            "counter = let x ~ 0 = add x 1 in x\n"
            "main :: *World -> *World\n"
            "main w = printInt counter w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 2";
        const char* test_source = ""
            "counter :: Maybe Int\n"
            "counter = x where\n"
            "  x ~ Nothing = x\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 3";
        const char* test_source = ""
            "counter :: (Int, Int)\n"
            "counter = x where\n"
            "  x ~ (0, 1) =\n"
            "    case x of\n"
            "      (l, r) -> (r, l)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 3.1";
        const char* test_source = ""
            "counter :: ((Int, Int), (Int, Int))\n"
            "counter = x where\n"
            "  x ~ ((0, 1), (2, 3)) =\n"
            "    case (gt mouseX 50, x) of\n"
            "      (True,  ((x, y), r)) -> (r, (y, x))\n"
            "      (False, ((z, w), r)) -> ((w, z), r)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 3.2";
        const char* test_source = ""
            "counter :: Maybe Int\n"
            "counter = x where\n"
            "  x ~ Nothing =\n"
            "    case x of\n"
            "      Nothing -> Just 0\n"
            "      Just 0  -> Nothing\n"
            "      Just i  -> Just (add i 1)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 1.5";
        const char* test_source = ""
            "counter :: Int\n"
            "counter =\n"
            "  case (gt mouseX 50) of\n"
            "    True  -> let x ~ 0 = (add x 1) in x\n"
            "    False -> let y ~ 0 = (sub y 1) in y\n"
            "main :: *World -> *World\n"
            "main w = printInt counter w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 1.6";
        const char* test_source = ""
            "counter :: (Int, Int)\n"
            "counter =\n"
            "  case (gt mouseX 50) of\n"
            "    True  -> let x ~ 0 = add x 1 in (x, x)\n"
            "    False -> let y ~ 0 = sub y 1 in (y, y)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 4";
        const char* test_source = ""
            "counter :: Int\n"
            "counter = add x y where\n"
            "  x ~ -10 = add x 1\n"
            "  y ~ 666 = add y 1\n"
            "main :: *World -> *World\n"
            "main w = printInt counter w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 5";
        const char* test_source = ""
            "counter :: (Int, Int, Int, Int)\n"
            "counter = x where\n"
            "  x ~ (0, 1, 2, 3) = x\n"
            "  y ~ (3, 2, 1, 0) = y\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 5.5";
        const char* test_source = ""
            "counter :: (Int, Int)\n"
            "counter = x where\n"
            "  x ~ (0, 1) =\n"
            "    case x of\n"
            "      (xl, xr) -> (xr, xl)\n"
            "  y ~ (2, 3) =\n"
            "    case y of\n"
            "      (yl, yr) -> (yr, yl)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 6";
        const char* test_source = ""
            "counter :: Int\n"
            "counter ~ 0 = add counter 1\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 7";
        const char* test_source = ""
            "counter :: Maybe Int\n"
            "counter ~ Nothing = \n"
            "  case counter of\n"
            "    Nothing -> Just 0\n"
            "    Just i  -> Just (add i 1)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 1";
        const char* test_source = ""
            "iopviously :: Int\n"
            "iopviously = add 1 0\n"
            "uopviously :: UInt\n"
            "uopviously = add 1 0\n"
            "fopviously :: Float\n"
            "fopviously = add 1 0\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 2";
        const char* test_source = ""
            "iopviously :: Int\n"
            "iopviously = sub 1 0\n"
            "uopviously :: UInt\n"
            "uopviously = sub 1 0\n"
            "fopviously :: Float\n"
            "fopviously = sub 1 0\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 3";
        const char* test_source = ""
            "iopviously :: Int\n"
            "iopviously = mul 1 0\n"
            "uopviously :: UInt\n"
            "uopviously = mul 1 0\n"
            "fopviously :: Float\n"
            "fopviously = mul 1 0\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 4";
        const char* test_source = ""
            "iopviously :: Int\n"
            "iopviously = abs 1 \n"
            "uopviously :: UInt\n"
            "uopviously = abs 1 \n"
            "fopviously :: Float\n"
            "fopviously = abs 1.0 \n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 5";
        const char* test_source = ""
            "iopviously :: Int\n"
            "iopviously = signum 1 \n"
            "uopviously :: UInt\n"
            "uopviously = signum 1 \n"
            "fopviously :: Float\n"
            "fopviously = signum 1.0 \n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 6";
        const char* test_source = ""
            "iopviously :: Int\n"
            "iopviously = fromInt 1 \n"
            "uopviously :: UInt\n"
            "uopviously = fromInt 1 \n"
            "fopviously :: Float\n"
            "fopviously = fromInt 1 \n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 7";
        const char* test_source = ""
            "iopviously :: Int -> Bool\n"
            "iopviously x = eq x x\n"
            "uopviously :: UInt -> Bool\n"
            "uopviously x = eq x x\n"
            "fopviously :: Float -> Bool\n"
            "fopviously x = eq x x\n"
            "nopviously :: () -> Bool\n"
            "nopviously x = eq x x\n"
            "bopviously :: Bool -> Bool\n"
            "bopviously x = eq x x\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 8";
        const char* test_source = ""
            "iopviously :: Int -> Bool\n"
            "iopviously x = neq x x\n"
            "uopviously :: UInt -> Bool\n"
            "uopviously x = neq x x\n"
            "fopviously :: Float -> Bool\n"
            "fopviously x = neq x x\n"
            "nopviously :: () -> Bool\n"
            "nopviously x = neq x x\n"
            "bopviously :: Bool -> Bool\n"
            "bopviously x = neq x x\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 9";
        const char* test_source = ""
            "iopviously :: Int -> Bool\n"
            "iopviously x = lt x x\n"
            "uopviously :: UInt -> Bool\n"
            "uopviously x = lt x x\n"
            "fopviously :: Float -> Bool\n"
            "fopviously x = lt x x\n"
            "nopviously :: () -> Bool\n"
            "nopviously x = lt x x\n"
            "bopviously :: Bool -> Bool\n"
            "bopviously x = lt x x\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 10";
        const char* test_source = ""
            "iopviously :: Int -> Bool\n"
            "iopviously x = gt x x\n"
            "uopviously :: UInt -> Bool\n"
            "uopviously x = gt x x\n"
            "fopviously :: Float -> Bool\n"
            "fopviously x = gt x x\n"
            "nopviously :: () -> Bool\n"
            "nopviously x = gt x x\n"
            "bopviously :: Bool -> Bool\n"
            "bopviously x = gt x x\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 11";
        const char* test_source = ""
            "iopviously :: Int -> Bool\n"
            "iopviously x = gte x x\n"
            "uopviously :: UInt -> Bool\n"
            "uopviously x = gte x x\n"
            "fopviously :: Float -> Bool\n"
            "fopviously x = gte x x\n"
            "nopviously :: () -> Bool\n"
            "nopviously x = gte x x\n"
            "bopviously :: Bool -> Bool\n"
            "bopviously x = gte x x\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 12";
        const char* test_source = ""
            "iopviously :: Int -> Bool\n"
            "iopviously x = lte x x\n"
            "uopviously :: UInt -> Bool\n"
            "uopviously x = lte x x\n"
            "fopviously :: Float -> Bool\n"
            "fopviously x = lte x x\n"
            "nopviously :: () -> Bool\n"
            "nopviously x = lte x x\n"
            "bopviously :: Bool -> Bool\n"
            "bopviously x = lte x x\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 13";
        const char* test_source = ""
            "andviously :: Bool\n"
            "andviously = True && False\n"
            "orviously :: Bool\n"
            "orviously = True || False\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

*/

    {
        const char* test_name   = "Op 1";
        const char* test_source = ""
            "x :: Int\n"
            "x = 1 + 2\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_mach_test_string(test_name, test_source);
    }

/*

    {
        const char* test_name   = "Undersaturate 1";
        const char* test_source = ""
            "x = eq True \n";
        necro_mach_test_string(test_name, test_source);
    }

*/


}

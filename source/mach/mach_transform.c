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

void                necro_core_transform_to_mach_1_go(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer);
void                necro_core_transform_to_mach_2_go(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer);
NecroMachAst*       necro_core_transform_to_mach_3_go(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer);

/*
    TODO:
        * Simple type check system for NecroMachAst
        * Double check prune polymorphic constructors?
*/


///////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////


///////////////////////////////////////////////////////
// Pass 1
///////////////////////////////////////////////////////
NecroMachAst* necro_core_transform_to_mach_1_data_con_dummy_type(NecroMachProgram* program, NecroMachAstSymbol* mach_ast_symbol, size_t max_arg_count)
{
    NecroArenaSnapshot snapshot = necro_snapshot_arena_get(&program->snapshot_arena);
    NecroMachType**    members  = necro_snapshot_arena_alloc(&program->snapshot_arena, (1 + max_arg_count) * sizeof(NecroMachType*));
    members[0]                  = program->necro_uint_type;
    for (size_t i = 1; i < max_arg_count + 1; ++i)
    {
        members[i] = program->necro_uint_type;
    }
    NecroMachAst* struct_def = necro_mach_create_struct_def(program, mach_ast_symbol, members, max_arg_count + 1);
    necro_snapshot_arena_rewind(&program->snapshot_arena, snapshot);
    return struct_def;
}

NecroMachAst* necro_core_transform_to_mach_1_data_con_type(NecroMachProgram* program, NecroMachAstSymbol* mach_ast_symbol, NecroCoreAst* core_ast)
{
    NecroArenaSnapshot snapshot = necro_snapshot_arena_get(&program->snapshot_arena);
    assert(core_ast->ast_type == NECRO_CORE_AST_DATA_CON);
    NecroType*      con_type  = necro_type_strip_for_all(core_ast->data_con.type);
    assert(con_type->type == NECRO_TYPE_CON || con_type->type == NECRO_TYPE_FUN);
    const size_t    arg_count = necro_type_arity(con_type);
    NecroMachType** members   = necro_snapshot_arena_alloc(&program->snapshot_arena, (1 + arg_count) * sizeof(NecroMachType*));
    members[0]                = program->necro_uint_type;
    for (size_t i = 1; i < arg_count + 1; ++i)
    {
        assert(con_type->type == NECRO_TYPE_FUN);
        members[i] = necro_mach_type_make_ptr_if_boxed(program, necro_mach_type_from_necro_type(program, con_type->fun.type1));
        con_type   = con_type->fun.type2;
    }
    NecroMachAst* struct_def = necro_mach_create_struct_def(program, mach_ast_symbol, members, arg_count + 1);
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
    // const char*         con_name        = necro_snapshot_arena_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_mk", core_ast->data_con.ast_symbol->name->str });
    NecroMachAstSymbol* con_symbol      = necro_mach_ast_symbol_gen(program, NULL, necro_snapshot_arena_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_mk", core_ast->data_con.ast_symbol->name->str }), NECRO_MANGLE_NAME);
    core_ast->data_con.ast_symbol->mach_symbol = con_symbol;
    // NecroMachAstSymbol* con_symbol      = necro_mach_ast_symbol_create(&program->arena, con_name);
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
    NecroMachAst*  data_ptr   = necro_mach_value_create_param_reg(program, mk_fn_def, 0);
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
        NecroMachAst* cast_ptr = necro_mach_build_bit_cast(program, mk_fn_def, data_ptr, necro_mach_type_create_ptr(&program->arena, struct_type));
        necro_mach_build_return(program, mk_fn_def, cast_ptr);
    }
    con_symbol->ast                            = mk_fn_def->fn_def.fn_value;
    con_symbol->state_type                     = NECRO_STATE_POLY;
    con_symbol->is_enum                        = false;
    con_symbol->is_constructor                 = true;
    con_symbol->is_primitive                   = false;
    con_symbol->con_num                        = con_number;
    core_ast->data_con.ast_symbol->mach_symbol = con_symbol;
    necro_snapshot_arena_rewind(&program->snapshot_arena, snapshot);
}

void necro_core_transform_to_mach_1_data_decl(NecroMachProgram* program, NecroCoreAst* core_ast)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_DATA_DECL);

    NecroArenaSnapshot snapshot = necro_snapshot_arena_get(&program->snapshot_arena);

    // Skip primitives
    if (core_ast->data_decl.ast_symbol->mach_symbol != NULL && core_ast->data_decl.ast_symbol->mach_symbol->is_primitive)
    {
        necro_snapshot_arena_rewind(&program->snapshot_arena, snapshot);
        return;
    }

    // Prune polymorphic types
    if (necro_type_is_polymorphic_ignoring_ownership(program->base, core_ast->data_decl.ast_symbol->type))
    {
        necro_snapshot_arena_rewind(&program->snapshot_arena, snapshot);
        return;
    }

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

    // Enum type early exit:
    if (max_arg_count == 0)
    {
        core_ast->data_decl.ast_symbol->mach_symbol->mach_type = program->necro_uint_type;
        cons                                                   = core_ast->data_decl.con_list;
        while (cons != NULL)
        {
            NecroCoreAst* con = cons->data;
            assert(con->ast_type == NECRO_CORE_AST_DATA_CON);
            con->data_con.ast_symbol->mach_symbol                  = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, core_ast->data_con.ast_symbol);
            core_ast->data_decl.ast_symbol->mach_symbol->mach_type = program->necro_uint_type;
            cons                                                   = cons->next;
        }
        necro_snapshot_arena_rewind(&program->snapshot_arena, snapshot);
        return;
    }

    // Create struct_defs
    NecroMachAst* struct_def = NULL;
    const bool   is_sum_type = core_ast->data_decl.con_list->next != NULL;
    if (is_sum_type)
    {
        struct_def = necro_core_transform_to_mach_1_data_con_dummy_type(program, core_ast->data_decl.ast_symbol->mach_symbol, max_arg_count);
        cons       = core_ast->data_decl.con_list;
        while (cons != NULL)
        {
            cons->data->data_con.ast_symbol->mach_symbol = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, cons->data->data_con.ast_symbol);
            necro_core_transform_to_mach_1_data_con_type(program, cons->data->data_con.ast_symbol->mach_symbol, cons->data);
            cons = cons->next;
        }
    }
    else
    {
        core_ast->data_decl.con_list->data->data_con.ast_symbol->mach_symbol = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, core_ast->data_con.ast_symbol);
        struct_def                                                           = necro_core_transform_to_mach_1_data_con_type(program, core_ast->data_decl.ast_symbol->mach_symbol, core_ast->data_decl.con_list->data);
    }

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
    necro_core_transform_to_mach_1_go(program, core_ast->bind.expr, machine_def);

    //---------------
    // declare mk_fn and init_fn
    if (outer == NULL)
    {
        // mk_fn
        {
            NecroMachAstSymbol* mk_fn_symbol = necro_mach_ast_symbol_gen(program, NULL, necro_snapshot_arena_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_mk", machine_def->machine_def.machine_name->name->str + 1 }), NECRO_MANGLE_NAME);
            NecroMachType*      mk_fn_type   = necro_mach_type_create_fn(&program->arena, necro_mach_type_create_void(&program->arena), NULL, 0);
            NecroMachAst*       mk_fn_body   = necro_mach_block_create(program, "entry", NULL);
            NecroMachAst*       mk_fn_def    = necro_mach_create_fn(program, mk_fn_symbol, mk_fn_body, mk_fn_type);
            program->functions.length--; // HACK: Don't want the mk function in the functions list, instead it belongs to the machine
            machine_def->machine_def.mk_fn   = mk_fn_def;
        }

        // init_fn
        {
            NecroMachAstSymbol* init_fn_symbol = necro_mach_ast_symbol_gen(program, NULL, necro_snapshot_arena_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_init", machine_def->machine_def.machine_name->name->str + 1 }), NECRO_MANGLE_NAME);
            NecroMachType*      init_fn_type   = necro_mach_type_create_fn(&program->arena, necro_mach_type_create_void(&program->arena), (NecroMachType*[]) { program->necro_uint_type }, 1);
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

void necro_core_transform_to_mach_1_case(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_CASE);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);
    assert(false && "TODO: Finish!");
}

void necro_core_transform_to_mach_1_for(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_FOR);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);
    assert(false && "TODO: Finish!");
}

void necro_core_transform_to_mach_1_go(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    switch (core_ast->ast_type)
    {
    case NECRO_CORE_AST_VAR:       return;
    case NECRO_CORE_AST_LIT:       return;
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

void necro_core_transform_to_mach_2_case(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_AST_CASE);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);
    assert(false && "TODO: Finish");
}

void necro_core_transform_to_mach_2_for(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_FOR);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);
    assert(false && "TODO: Finish!");
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

NecroMachSlot necro_mach_add_member(NecroMachProgram* program, NecroMachDef* machine_def, NecroMachType* new_member, NecroMachAst* member_ast)
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
    NecroMachSlot slot                             = (NecroMachSlot) { .necro_machine_type = new_member, .slot_num = machine_def->num_members, .slot_ast = member_ast, .machine_def = machine_def };
    machine_def->members[machine_def->num_members] = slot;
    machine_def->num_members++;
    return slot;
}

void necro_mach_remove_only_self_recursive_member(NecroMachProgram* program, NecroMachAst* ast)
{
    assert(program != NULL);
    assert(ast->type == NECRO_MACH_DEF);
    // TODO / NOTE: Does comparing the element type likes this work? Shouldn't this use a structural equality, not a pointer equality?
    if (ast->machine_def.num_arg_names == 0 || ast->machine_def.num_members != 1 || ast->machine_def.members[0].necro_machine_type->ptr_type.element_type != ast->necro_machine_type)
        return;
    ast->machine_def.num_members = 0;
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
        necro_core_transform_to_mach_2_go(program, core_ast->bind.expr, outer);
        necro_mach_calculate_statefulness(program, machine_def);
        return;
    }
    machine_def->necro_machine_type = necro_mach_type_create_struct(&program->arena, machine_def->machine_def.machine_name, NULL, 0);
    necro_core_transform_to_mach_2_go(program, core_ast->bind.expr, machine_def);

    //--------------------
    // Calculate statefulness
    necro_mach_remove_only_self_recursive_member(program, machine_def);
    necro_mach_calculate_statefulness(program, machine_def);

    //--------------------
    // Calculate machine type
    const bool      is_machine_fn        = machine_def->machine_def.num_arg_names > 0;
    const size_t    num_machine_members  = machine_def->machine_def.num_members;
    NecroMachType** machine_type_members = necro_paged_arena_alloc(&program->arena, num_machine_members * sizeof(NecroMachType*));
    for (size_t i = 0; i < machine_def->machine_def.num_members; ++i)
        machine_type_members[i] = machine_def->machine_def.members[i].necro_machine_type;
    machine_def->necro_machine_type->struct_type.members     = machine_type_members;
    machine_def->necro_machine_type->struct_type.num_members = num_machine_members;

    //--------------------
    // Add global state
    // if (outer == NULL && !is_machine_fn && machine_def->machine_def.state_type == NECRO_STATE_STATEFUL)
    if (outer == NULL && !is_machine_fn && machine_def->machine_def.num_members > 0)
    {
        NecroMachAst* global_state            = necro_mach_value_create_global(program, machine_def->machine_def.state_name, necro_mach_type_create_ptr(&program->arena, necro_mach_type_create_ptr(&program->arena, machine_def->necro_machine_type)));
        machine_def->machine_def.global_state = global_state;
        necro_mach_program_add_global(program, global_state);
    }

    //--------------------
    // Add global value
    if (outer == NULL && !is_machine_fn)
    {
        NecroMachAst* global_value = NULL;
        if (necro_mach_type_is_unboxed(program, machine_def->machine_def.value_type))
            global_value = necro_mach_value_create_global(program, machine_def->machine_def.machine_name, necro_mach_type_create_ptr(&program->arena, machine_def->machine_def.value_type));
        else
            global_value = necro_mach_value_create_global(program, machine_def->machine_def.machine_name, necro_mach_type_create_ptr(&program->arena, necro_mach_type_create_ptr(&program->arena, machine_def->machine_def.value_type)));
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
        {
            machine_def->machine_def.init_fn->necro_machine_type->fn_type.parameters[0] = machine_ptr_type;
            NecroMachAst* data_ptr = necro_mach_value_create_param_reg(program, machine_def->machine_def.init_fn, 0);
            // gep and init
            for (size_t i = 0; i < machine_def->machine_def.num_members; ++i)
            {
                NecroMachSlot slot = machine_def->machine_def.members[i];
                if (slot.slot_ast->type == NECRO_MACH_DEF && slot.slot_ast->machine_def.init_fn != NULL)
                {
                    assert(i <= UINT32_MAX);
                    NecroMachAst* member = necro_mach_build_gep(program, machine_def->machine_def.init_fn, data_ptr, (uint32_t[]) { 0, (uint32_t) i }, 2, "member");
                    necro_mach_build_call(program, machine_def->machine_def.init_fn, slot.slot_ast->machine_def.init_fn->fn_def.fn_value, (NecroMachAst*[]) { member }, 1, NECRO_LANG_CALL, "");
                }
            }
            necro_mach_build_return_void(program, machine_def->machine_def.init_fn);
        }

        //--------------------
        // mk_fn
        {
            // TODO: Double check how we're doing slot calculations!
            machine_def->machine_def.mk_fn->necro_machine_type->fn_type.return_type = machine_ptr_type;
            const size_t  num_slots = necro_mach_type_calculate_num_slots(machine_def->necro_machine_type);
            NecroMachAst* data_ptr  = necro_mach_build_nalloc(program, machine_def->machine_def.mk_fn, machine_def->necro_machine_type, num_slots);
            necro_mach_build_call(program, machine_def->machine_def.mk_fn, machine_def->machine_def.init_fn->fn_def.fn_value, (NecroMachAst*[]) { data_ptr }, 1, NECRO_LANG_CALL, "");
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

// TODO: Need to handl Nullary constructors in Sum Unions here!
void necro_core_transform_to_mach_2_var(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_VAR);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);
    NecroMachAstSymbol* mach_symbol = core_ast->var.ast_symbol->mach_symbol;
    if (mach_symbol->is_enum)
        return;
    NecroMachAst* machine_ast = mach_symbol->ast;
    if (machine_ast == NULL)
    {
        // Lambda args are NULL at first...better way to differentiate!?
        // TODO / NEW NOTE: Is this still true?
        // If so why not simply fill them in during pass 1?
        // assert(false);
        return;
    }
    // TODO / NOTE: We should also handle constructors here, as constructors will also create slots for memory!!!!!!!!!!!
    if (machine_ast->type == NECRO_MACH_DEF)
    {
        // if (machine_ast->machine_def.state_type != NECRO_STATE_STATEFUL)
        if (machine_ast->machine_def.num_members == 0)
            return;
        if (necro_mach_is_var_machine_arg(outer, core_ast->var.ast_symbol->name))
            return;
        if (machine_ast->machine_def.outer == NULL)
            return;
        if (machine_ast->machine_def.is_persistent_slot_set)
            return;
        machine_ast->machine_def.is_persistent_slot_set = true;
        mach_symbol->slot = necro_mach_type_is_unboxed(program, machine_ast->machine_def.value_type)
            ? necro_mach_add_member(program, &outer->machine_def, machine_ast->machine_def.value_type, machine_ast)
            : necro_mach_add_member(program, &outer->machine_def, necro_mach_type_create_ptr(&program->arena, machine_ast->machine_def.value_type), machine_ast);
    }
    else if (mach_symbol->is_constructor)
    {
        assert(mach_symbol->ast->necro_machine_type->type == NECRO_MACH_TYPE_FN);
        assert(mach_symbol->ast->necro_machine_type->fn_type.num_parameters == 1);
        assert(mach_symbol->ast->necro_machine_type->fn_type.parameters[0]->type == NECRO_MACH_TYPE_PTR);
        NecroMachType* con_type = mach_symbol->ast->necro_machine_type->fn_type.parameters[0]->ptr_type.element_type;
        // mach_symbol->slot            = necro_mach_add_member(program, &outer->machine_def, con_type, mach_symbol->ast);
        necro_mach_add_member(program, &outer->machine_def, con_type, mach_symbol->ast);
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
    NecroMachAstSymbol* symbol        = function->var.ast_symbol->mach_symbol;
    assert(symbol != NULL);
    NecroMachAst*       fn_value      = symbol->ast;
    assert(fn_value != NULL);
    NecroMachType*      fn_type       = NULL;
    if (fn_value->type == NECRO_MACH_DEF)
    {
        fn_type = fn_value->machine_def.fn_type;
        // if (fn_value->machine_def.state_type == NECRO_STATE_STATEFUL)
        if (fn_value->machine_def.num_members > 0)
            symbol->slot = necro_mach_add_member(program, &outer->machine_def, fn_value->necro_machine_type, fn_value);
    }
    else if (symbol->is_constructor)
    {
        assert(fn_value->necro_machine_type->type == NECRO_MACH_TYPE_FN);
        assert(fn_value->necro_machine_type->fn_type.num_parameters > 1);
        assert(fn_value->necro_machine_type->fn_type.parameters[0]->type == NECRO_MACH_TYPE_PTR);
        fn_type                 = fn_value->necro_machine_type;
        NecroMachType* con_type = fn_value->necro_machine_type->fn_type.parameters[0]->ptr_type.element_type;
        necro_mach_add_member(program, &outer->machine_def, con_type, fn_value);
        // symbol->slot            = necro_mach_add_member(program, &outer->machine_def, con_type, fn_value);
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
    case NECRO_CORE_AST_LIT:       return;
    case NECRO_CORE_AST_DATA_DECL: return;
    case NECRO_CORE_AST_DATA_CON:  return;
    default:                       assert(false && "Unimplemented AST type in necro_core_transform_to_mach_2_go"); return;
    }
}

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
    // Construct main
    // TODO: Move to necro_mach_construct_main
    NecroAstSymbol* ast_symbol = necro_symtable_get_top_level_ast_symbol(program->base->scoped_symtable, necro_intern_string(program->intern, "main"));
    if (ast_symbol != NULL)
    {
        // TODO: Handle no main in program
        program->program_main = ast_symbol->core_ast_symbol->mach_symbol->ast;
    }

    if (info.compilation_phase == NECRO_PHASE_TRANSFORM_TO_MACHINE && info.verbosity > 0)
        necro_mach_print_program(program);
}


///////////////////////////////////////////////////////
// Testing
///////////////////////////////////////////////////////
#define NECRO_MACH_TEST_VERBOSE 1
void necro_mach_test_string(const char* test_name, const char* str)
{
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
    // TODO: defunctionalization here!
    necro_core_state_analysis(info, &intern, &base, &core_ast);
    necro_core_transform_to_mach(info, &intern, &base, &core_ast, &mach_program);

    // Print
#if NECRO_MACH_TEST_VERBOSE
    // printf("\n");
    // necro_core_ast_pretty_print(core_ast.root);
    printf("\n");
    necro_mach_print_program(&mach_program);
#endif
    printf("NecroMach %s test: Passed\n", test_name);
    fflush(stdout);

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
        const char* test_name   = "Basic 1";
        const char* test_source = ""
            "x = True\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Data 1";
        const char* test_source = ""
            "data TwoInts = TwoInts Int Int\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Data 2";
        const char* test_source = ""
            "data TwoInts  = TwoInts Int Int\n"
            "data DoubleUp = DoubleUp TwoInts TwoInts\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Data 3";
        const char* test_source = ""
            "data TwoInts   = TwoInts Int Int\n"
            "data DoubleUp  = DoubleUp TwoInts TwoInts\n"
            "data TwoOrFour = Two TwoInts | Four DoubleUp DoubleUp\n";
        necro_mach_test_string(test_name, test_source);
    }


    {
        const char* test_name   = "Bind 1";
        const char* test_source = ""
            "data TwoInts = TwoInts Int Int\n"
            "twoForOne :: Int -> TwoInts\n"
            "twoForOne i = TwoInts i i\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Bind 2";
        const char* test_source = ""
            "data TwoInts   = TwoInts Int Int\n"
            "data DoubleUp  = DoubleUp TwoInts TwoInts\n"
            "doubleDown :: Int -> DoubleUp\n"
            "doubleDown i = DoubleUp (TwoInts i i) (TwoInts i i)\n";
        necro_mach_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Bind 3";
        const char* test_source = ""
            "data TwoOrNone = None | TwoInts Int Int \n"
            "nope :: Int -> TwoOrNone\n"
            "nope i = None\n";
        necro_mach_test_string(test_name, test_source);
    }

*/

    {
        const char* test_name   = "Bind 4";
        const char* test_source = ""
            "data TwoInts  = TwoInts Int Int\n"
            "data DoubleUp = DoubleUp TwoInts TwoInts\n"
            "data Quad     = Quad DoubleUp DoubleUp\n"
            "doubleDown :: Int -> DoubleUp\n"
            "doubleDown i = DoubleUp (TwoInts i i) (TwoInts i i)\n"
            "fourForOne :: Quad\n"
            "fourForOne = Quad (doubleDown 100) (doubleDown 200)\n";
        necro_mach_test_string(test_name, test_source);
    }

    // // TODO: This isn't monomorphizing!?!?!?!
    // {
    //     const char* test_name   = "Data 4";
    //     const char* test_source = ""
    //         "notThere :: Maybe Int\n"
    //         "notThere = Nothing\n";
    //     necro_mach_test_string(test_name, test_source);
    // }

}

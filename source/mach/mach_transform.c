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
        * phi type checking!
        * Single statement blocks seems broken?!!?!?
        * Finish up case
        * Polymorphic type handling and resolution (Plus perhaps type uniqueing)
        * Simple type check system for NecroMachAst
        * For loops
        * Stateful machines / BindRec?
        * Codegen
*/

///////////////////////////////////////////////////////
// Pass 1
///////////////////////////////////////////////////////
NecroMachAst* necro_core_transform_to_mach_1_data_con_dummy_type(NecroMachProgram* program, NecroMachAstSymbol* mach_ast_symbol, size_t max_arg_count)
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

NecroMachAst* necro_core_transform_to_mach_1_data_con_type(NecroMachProgram* program, NecroMachAstSymbol* mach_ast_symbol, NecroCoreAst* core_ast)
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
    if (core_ast->data_decl.ast_symbol->mach_symbol != NULL && core_ast->data_decl.ast_symbol->mach_symbol->is_primitive)
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
    necro_core_transform_to_mach_1_go(program, core_ast->bind.expr, machine_def);

    //---------------
    // declare mk_fn and init_fn
    if (outer == NULL)
    {
        // mk_fn
        {
            NecroMachAstSymbol* mk_fn_symbol = necro_mach_ast_symbol_gen(program, NULL, necro_snapshot_arena_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_mk", machine_def->machine_def.machine_name->name->str + 1 }), NECRO_MANGLE_NAME);
            NecroMachType*      mk_fn_type   = necro_mach_type_create_fn(&program->arena, necro_mach_type_create_void(program), NULL, 0);
            NecroMachAst*       mk_fn_body   = necro_mach_block_create(program, "entry", NULL);
            NecroMachAst*       mk_fn_def    = necro_mach_create_fn(program, mk_fn_symbol, mk_fn_body, mk_fn_type);
            program->functions.length--; // HACK: Don't want the mk function in the functions list, instead it belongs to the machine
            machine_def->machine_def.mk_fn   = mk_fn_def;
        }

        // init_fn
        {
            NecroMachAstSymbol* init_fn_symbol = necro_mach_ast_symbol_gen(program, NULL, necro_snapshot_arena_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_init", machine_def->machine_def.machine_name->name->str + 1 }), NECRO_MANGLE_NAME);
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

void necro_core_transform_to_mach_1_for(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_FOR);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);
    assert(false && "TODO: Finish!");
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

void necro_core_transform_to_mach_1_go(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    switch (core_ast->ast_type)
    {
    case NECRO_CORE_AST_LIT:       return;
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
    // TODO / NOTE: Using structural equality instead of pointer equality
    // if (ast->machine_def.num_arg_names == 0 || ast->machine_def.num_members != 1 || ast->machine_def.members[0].necro_machine_type->ptr_type.element_type != ast->necro_machine_type)
    if (ast->machine_def.num_arg_names == 0 || ast->machine_def.num_members != 1 || !necro_mach_type_is_eq(ast->machine_def.members[0].necro_machine_type->ptr_type.element_type, ast->necro_machine_type))
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

    // //--------------------
    // // Add global state
    // // if (outer == NULL && !is_machine_fn && machine_def->machine_def.state_type == NECRO_STATE_STATEFUL)
    // if (outer == NULL && !is_machine_fn && machine_def->machine_def.num_members > 0)
    // {
    //     NecroMachAst* global_state            = necro_mach_value_create_global(program, machine_def->machine_def.state_name, necro_mach_type_create_ptr(&program->arena, necro_mach_type_create_ptr(&program->arena, machine_def->necro_machine_type)));
    //     machine_def->machine_def.global_state = global_state;
    //     necro_mach_program_add_global(program, global_state);
    // }

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
                    necro_mach_build_call(program, machine_def->machine_def.init_fn, slot.slot_ast->machine_def.init_fn->fn_def.fn_value, (NecroMachAst*[]) { member }, 1, NECRO_MACH_CALL_LANG, "");
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
    if (machine_ast->type == NECRO_MACH_DEF)
    {
        // if (machine_ast->machine_def.state_type != NECRO_STATE_STATEFUL)
        // if (machine_ast->machine_def.num_members == 0)
        if (machine_ast->machine_def.state_type != NECRO_STATE_STATEFUL || machine_ast->machine_def.num_members == 0) // Add member if either?
            return;
        if (necro_mach_is_var_machine_arg(outer, core_ast->var.ast_symbol->name))
            return;
        if (machine_ast->machine_def.outer == NULL)
            return;
        if (machine_ast->machine_def.is_persistent_slot_set)
            return;
        machine_ast->machine_def.is_persistent_slot_set = true;
        core_ast->var.persistent_slot = necro_mach_type_is_unboxed(program, machine_ast->machine_def.value_type)
            ? necro_mach_add_member(program, &outer->machine_def, machine_ast->machine_def.value_type, machine_ast).slot_num
            : necro_mach_add_member(program, &outer->machine_def, necro_mach_type_create_ptr(&program->arena, machine_ast->machine_def.value_type), machine_ast).slot_num;
    }
    else if (mach_symbol->is_constructor)
    {
        assert(mach_symbol->ast->necro_machine_type->type == NECRO_MACH_TYPE_FN);
        assert(mach_symbol->ast->necro_machine_type->fn_type.num_parameters == 1);
        assert(mach_symbol->ast->necro_machine_type->fn_type.parameters[0]->type == NECRO_MACH_TYPE_PTR);
        NecroMachType* con_type = mach_symbol->ast->necro_machine_type->fn_type.parameters[0]->ptr_type.element_type;
        core_ast->var.persistent_slot = necro_mach_add_member(program, &outer->machine_def, con_type, mach_symbol->ast).slot_num;
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
        if (fn_value->machine_def.num_members > 0)
            core_ast->app.persistent_slot = necro_mach_add_member(program, &outer->machine_def, fn_value->necro_machine_type, fn_value).slot_num;
    }
    else if (symbol->is_constructor)
    {
        assert(fn_value->necro_machine_type->type == NECRO_MACH_TYPE_FN);
        assert(fn_value->necro_machine_type->fn_type.num_parameters > 1);
        assert(fn_value->necro_machine_type->fn_type.parameters[0]->type == NECRO_MACH_TYPE_PTR);
        fn_type                 = fn_value->necro_machine_type;
        NecroMachType* con_type = fn_value->necro_machine_type->fn_type.parameters[0]->ptr_type.element_type;
        core_ast->app.persistent_slot = necro_mach_add_member(program, &outer->machine_def, con_type, fn_value).slot_num;
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
    case NECRO_AST_CONSTANT_INTEGER: return necro_mach_value_create_word_int(program, core_ast->lit.int_literal);
    case NECRO_AST_CONSTANT_FLOAT:   return necro_mach_value_create_word_float(program, core_ast->lit.float_literal);
    case NECRO_AST_CONSTANT_CHAR:    return necro_mach_value_create_word_uint(program, core_ast->lit.char_literal);
    default:                         return NULL;
    }
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

NecroMachAst* necro_core_transform_to_mach_3_for(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_FOR);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);
    assert(false && "TODO");
    return NULL;
}

NecroMachAst* necro_core_transform_to_mach_3_var(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_VAR);
    assert(outer != NULL);
    assert(outer->type == NECRO_MACH_DEF);
    NecroMachAstSymbol* symbol = core_ast->var.ast_symbol->mach_symbol;
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
    // Constructor
    else if (symbol->is_constructor)// && symbol->arity == 0) // NOTE: Not checking arity as theoretically that should already be caught be lambda lifting, etc
    {
        NecroMachAst* value_ptr = necro_mach_build_gep(program, outer->machine_def.update_fn, necro_mach_value_create_param_reg(program, outer->machine_def.update_fn, 0), (uint32_t[]) { 0, core_ast->var.persistent_slot }, 2, "prs");
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
        // TODO / NOTE: This system probably needs to be updated to do a full deep copy of object instead of a shallow copy, otherwise the innards will get stomped on with the next call to update_fn
        // const size_t   slot_num  = symbol->slot.slot_num;
        NecroMachType* slot_type = outer->necro_machine_type->struct_type.members[core_ast->var.persistent_slot];
        NecroMachAst*  value_ptr = necro_mach_build_gep(program, outer->machine_def.update_fn, necro_mach_value_create_param_reg(program, outer->machine_def.update_fn, 0), (uint32_t[]) { 0, core_ast->var.persistent_slot }, 2, "prs");
        if (necro_mach_type_is_unboxed(program, slot_type) || slot_type->type == NECRO_MACH_TYPE_PTR)
            return necro_mach_build_load(program, outer->machine_def.update_fn, value_ptr, "val");
        else
            return value_ptr;
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
    size_t        persistent_slot = core_ast->app.persistent_slot;
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
    NecroMachAst*        fn_value   = function->var.ast_symbol->mach_symbol->ast;
    bool                 uses_state = false;
    assert(fn_value != NULL);
    if (fn_value->type == NECRO_MACH_DEF)
    {
        fn_value    = fn_value->machine_def.update_fn->fn_def.fn_value;
        // uses_state  = machine_def->machine_def.state_type == NECRO_STATE_STATEFUL;
        uses_state  = fn_value->machine_def.num_members > 0;
    }
    else if (function->var.ast_symbol->mach_symbol->is_constructor)
    {
        uses_state = true;
        // fn_value = fn_value->fn_def.fn_value; // TODO: Don't believe we need this!?!?!?
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
        assert(persistent_slot <= UINT32_MAX);
        args[0] = necro_mach_build_gep(program, outer->machine_def.update_fn, necro_mach_value_create_param_reg(program, outer->machine_def.update_fn, 0), (uint32_t[]) { 0, (uint32_t) persistent_slot }, 2, "state");
    }
    //--------------------
    // Call fn
    //--------------------
    NecroMachAst* result = necro_mach_build_call(program, outer->machine_def.update_fn, fn_value, args, arg_count, call_type, "app");
    necro_snapshot_arena_rewind(&program->snapshot_arena, snapshot);
    return result;
}

NecroMachAst* necro_core_transform_to_mach_3_bind(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_BIND);
    if (outer != NULL)
        assert(outer->type == NECRO_MACH_DEF);

    if (core_ast->bind.ast_symbol->mach_symbol->is_primitive)
        return NULL;

    //--------------------
    // Retrieve machine_def
    NecroMachAstSymbol* symbol      = core_ast->bind.ast_symbol->mach_symbol;
    NecroMachAst*       machine_def = symbol->ast;
    assert(machine_def != NULL);
    assert(machine_def->type == NECRO_MACH_DEF);

    if (outer != NULL)
    {
        NecroMachAst* return_value = necro_core_transform_to_mach_3_go(program, core_ast->bind.expr, outer);
        if (machine_def->machine_def.is_persistent_slot_set)
        {
            assert(false && "TODO: Finish!");
            // TODO: We need to do a deep copy into state object, not a simple pointer copy!
            // Outer update_fn is NULL somehow?!?!?!
            const size_t  slot     = 0; // TODO: FIX!
            NecroMachAst* param0   = necro_mach_value_create_param_reg(program, outer->machine_def.update_fn, 0);
            NecroMachAst* slot_ptr = necro_mach_build_gep(program, outer->machine_def.update_fn, param0, (uint32_t[]) { 0, slot }, 2, "prs");
            // necro_build_store_into_slot(program, machine_def->machine_def.outer->machine_def.update_fn, return_value, param0, info->persistent_slot);
            necro_mach_build_store(program, machine_def->machine_def.outer->machine_def.update_fn, return_value, slot_ptr);
        }
        else
        {
            symbol->ast = return_value;
        }
        return return_value;
    }

    //--------------------
    // Start function
    NecroArenaSnapshot  snapshot          = necro_snapshot_arena_get(&program->snapshot_arena);
    bool                uses_state        = machine_def->machine_def.num_members > 0;
    NecroMachAstSymbol* update_symbol     = necro_mach_ast_symbol_gen(program, NULL, necro_snapshot_arena_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_update", machine_def->machine_def.machine_name->name->str + 1 }), NECRO_MANGLE_NAME);
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
    NecroMachType* update_fn_type      = necro_mach_type_create_fn(&program->arena, necro_mach_type_make_ptr_if_boxed(program, machine_def->machine_def.value_type), update_params, num_update_params);
    NecroMachAst*  update_fn_body      = necro_mach_block_create(program, "entry", NULL);
    NecroMachAst*  update_fn_def       = necro_mach_create_fn(program, update_symbol, update_fn_body, update_fn_type);
    program->functions.length--; // HACK: Don't want the update function in the functions list, instead it belongs to the machine
    machine_def->machine_def.update_fn = update_fn_def;
    for (size_t i = 0; i < machine_def->machine_def.num_arg_names; ++i)
    {
        machine_def->machine_def.arg_names[i]->ast = necro_mach_value_create_param_reg(program, update_fn_def, i + (uses_state ? 1 : 0));
    }

    //--------------------
    // Go deeper
    NecroMachAst* result = necro_core_transform_to_mach_3_go(program, core_ast->bind.expr, machine_def);

    //--------------------
    // Finish function
    necro_mach_build_return(program, update_fn_def, result);
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
        program->program_main = main_symbol->core_ast_symbol->mach_symbol->ast;
    }

    //--------------------
    // Declare NecroMain
    NecroMachAstSymbol* necro_main_symbol = necro_mach_ast_symbol_gen(program, NULL, "_necro_main", NECRO_MANGLE_NAME);
    NecroMachType*      necro_main_type   = necro_mach_type_create_fn(&program->arena, necro_mach_type_create_void(program), NULL, 0);
    NecroMachAst*       necro_main_entry  = necro_mach_block_create(program, "entry", NULL);
    NecroMachAst*       necro_main_fn     = necro_mach_create_fn(program, necro_main_symbol, necro_main_entry, necro_main_type);
    program->functions.length--; // Hack...
    NecroMachAst*       necro_main_loop   = necro_mach_block_append(program, necro_main_fn, "loop");

    //--------------------
    // entry
    necro_mach_build_break(program, necro_main_fn, necro_main_loop);
    necro_mach_build_call(program, necro_main_fn, program->runtime._necro_init_runtime->ast->fn_def.fn_value, NULL, 0, NECRO_MACH_CALL_C, "");

    //--------------------
    // Create states
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        if (program->machine_defs.data[i]->machine_def.num_members == 0 || program->machine_defs.data[i]->machine_def.num_arg_names != 0)
            continue;
        // NecroMachAst* mk_result = necro_mach_build_call(program, necro_main_fn, program->machine_defs.data[i]->machine_def.mk_fn->fn_def.fn_value, NULL, 0, NECRO_MACH_CALL_LANG, "state");
        // necro_mach_build_store(program, necro_main_fn, mk_result, program->machine_defs.data[i]->machine_def.global_state);
        program->machine_defs.data[i]->machine_def.global_state = necro_mach_build_call(program, necro_main_fn, program->machine_defs.data[i]->machine_def.mk_fn->fn_def.fn_value, NULL, 0, NECRO_MACH_CALL_LANG, "state");
    }
    if (program->program_main != NULL && program->program_main->machine_def.num_members > 0)
    {
        // NecroMachAst* global_state = necro_mach_value_create_global(program, program->program_main->machine_def.state_name, necro_mach_type_create_ptr(&program->arena, necro_mach_type_create_ptr(&program->arena, program->program_main->necro_machine_type)));
        // program->program_main->machine_def.global_state = global_state;
        // necro_mach_program_add_global(program, global_state);
        // NecroMachAst* mk_result = necro_mach_build_call(program, necro_main_fn, program->program_main->machine_def.mk_fn->fn_def.fn_value, NULL, 0, NECRO_MACH_CALL_LANG, "main_state");
        // necro_mach_build_store(program, necro_main_fn, mk_result, program->program_main->machine_def.global_state);
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
            // NecroMachAst* state  = necro_mach_build_load(program, necro_main_fn, program->machine_defs.data[i]->machine_def.global_state, "state");
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
    necro_mach_build_call(program, necro_main_fn, program->runtime._necro_update_runtime->ast->fn_def.fn_value, NULL, 0, NECRO_MACH_CALL_C, "");
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        if (program->machine_defs.data[i]->machine_def.state_type == NECRO_STATE_CONSTANT || program->machine_defs.data[i]->machine_def.num_arg_names != 0)
            continue;
        if (program->machine_defs.data[i]->machine_def.num_members > 0)
        {
            // NecroMachAst* state  = necro_mach_build_load(program, necro_main_fn, program->machine_defs.data[i]->machine_def.global_state, "state");
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
            // NecroMachAst* state  = necro_mach_build_load(program, necro_main_fn, program->program_main->machine_def.global_state, "state");
            NecroMachAst* state  = program->program_main->machine_def.global_state;
            NecroMachAst* result = necro_mach_build_call(program, necro_main_fn, program->program_main->machine_def.update_fn->fn_def.fn_value, (NecroMachAst*[]) { state, world_value }, 2, NECRO_MACH_CALL_LANG, "main_result");
            UNUSED(result);
            // necro_mach_build_store(program, necro_main_fn, result, program->program_main->machine_def.global_value);
        }
        else
        {
            NecroMachAst* result = necro_mach_build_call(program, necro_main_fn, program->program_main->machine_def.update_fn->fn_def.fn_value, (NecroMachAst*[]) { world_value }, 1, NECRO_MACH_CALL_LANG, "main_result");
            UNUSED(result);
            // necro_mach_build_store(program, necro_main_fn, result, program->program_main->machine_def.global_value);
        }
    }

    //--------------------
    // Clean up / Loop
    necro_mach_build_call(program, necro_main_fn, program->runtime._necro_sleep->ast->fn_def.fn_value, (NecroMachAst*[]) { necro_mach_value_create_uint32(program, 10) }, 1, NECRO_MACH_CALL_C, "");
    necro_mach_build_break(program, necro_main_fn, necro_main_loop);
    program->necro_main = necro_main_fn;
    // necro_mach_build_return_void(program, necro_main_fn);
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
    // TODO: defunctionalization here!
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
        const char* test_name   = "Case 1";
        const char* test_source = ""
            "main w =\n"
            "  case False of\n"
            "    True  -> printInt 0 w\n"
            "    _     -> printInt 1 w\n";
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
*/

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

    // TODO: Case is getting dropped in core ast's?!?!
    // // TODO: This isn't monomorphizing!?!?!?!
    // {
    //     const char* test_name   = "Data 4";
    //     const char* test_source = ""
    //         "notThere :: Maybe Int\n"
    //         "notThere = Nothing\n";
    //     necro_mach_test_string(test_name, test_source);
    // }

}

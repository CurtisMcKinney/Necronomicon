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
    const char*         con_name        = necro_snapshot_arena_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_mk", core_ast->data_con.ast_symbol->name->str });
    NecroMachAstSymbol* con_symbol      = necro_mach_ast_symbol_create(&program->arena, necro_intern_string(program->intern, con_name));
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
    con_symbol->ast = mk_fn_def->fn_def.fn_value;
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
    UNUSED(outer);
}

void necro_core_transform_to_mach_1_bind_rec(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_BIND_REC);
    UNUSED(outer);
}

void necro_core_transform_to_mach_1_app(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_APP);
    UNUSED(outer);
}

void necro_core_transform_to_mach_1_lam(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_LAM);
    UNUSED(outer);
}

void necro_core_transform_to_mach_1_let(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_LET);
    UNUSED(outer);
}

void necro_core_transform_to_mach_1_case(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_CASE);
    UNUSED(outer);
}

void necro_core_transform_to_mach_1_for(NecroMachProgram* program, NecroCoreAst* core_ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->ast_type == NECRO_CORE_AST_FOR);
    UNUSED(outer);
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

void necro_core_transform_to_mach(NecroCompileInfo info, NecroIntern* intern, NecroBase* base, NecroCoreAstArena* core_ast_arena, NecroMachProgram* program)
{
    *program = necro_mach_program_create(intern, base);

    // Pass 1
    NecroCoreAst* top = core_ast_arena->root;
    while (top != NULL && top->ast_type == NECRO_CORE_AST_LET)
    {
        necro_core_transform_to_mach_1_go(program, top->let.bind, NULL);
        top = top->let.expr;
    }
    if (top != NULL)
        necro_core_transform_to_mach_1_go(program, top, NULL);

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
*/

    {
        const char* test_name   = "Data 3";
        const char* test_source = ""
            "data TwoInts   = TwoInts Int Int\n"
            "data DoubleUp  = DoubleUp TwoInts TwoInts\n"
            "data TwoOrFour = Two TwoInts | Four DoubleUp DoubleUp\n";
        necro_mach_test_string(test_name, test_source);
    }

/*
*/

    // // TODO: This isn't monomorphizing!?!?!?!
    // {
    //     const char* test_name   = "Data 4";
    //     const char* test_source = ""
    //         "notThere :: Maybe Int\n"
    //         "notThere = Nothing\n";
    //     necro_mach_test_string(test_name, test_source);
    // }

}

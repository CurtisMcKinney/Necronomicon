/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include "utility.h"
#include "lexer.h"
#include "parser.h"
#include "intern.h"
#include "runtime.h"
#include "vault.h"
#include "symtable.h"
#include "ast.h"
#include "renamer.h"
#include "type/type.h"
#include "type/infer.h"
#include "type/prim.h"
#include "type/type_class.h"
#include "utility/hash_table.h"
#include "d_analyzer.h"
#include "driver.h"
#include "core/core.h"
#include "core/closure_conversion.h"
#include "machine/machine.h"
#include "machine/machine_print.h"
#include "codegen/codegen_llvm.h"
#include "core/core_pretty_print.h"

void necro_compile_impl(
    const char* input_string,
    bool should_optimize,
    NECRO_PHASE compilation_phase,
    NecroInfer* infer,
    NecroParser* parser,
    NecroTransformToCore* core_transform,
    NecroLexer* lexer,
    NecroCodeGenLLVM* codegen_llvm,
    NecroAST* ast,
    NecroAST_Reified* ast_r,
    NecroCoreAST* ast_core,
    NecroMachineProgram* machine,
    uint32_t* destruct_flags)
{
    if (compilation_phase == NECRO_PHASE_NONE)
        return;

    struct NecroTimer* timer = necro_create_timer();

    //=====================================================
    // Lexing, PRE - Layout
    //=====================================================
    if (compilation_phase != NECRO_PHASE_JIT)
        necro_announce_phase("Lexing");
    necro_start_timer(timer);
    *lexer = necro_create_lexer(input_string);
    *destruct_flags |= BIT(NECRO_PHASE_LEX);
    if (necro_lex(lexer) != NECRO_SUCCESS)
    {
        necro_print_error(&lexer->error, input_string, "Syntax");
        return;
    }
    if (compilation_phase == NECRO_PHASE_LEX_PRE_LAYOUT)
        necro_print_lexer(lexer);
    if (compilation_phase == NECRO_PHASE_LEX_PRE_LAYOUT)
        return;

    //=====================================================
    // Lexing, Layout
    //=====================================================
    if (necro_lex_fixup_layout(lexer) != NECRO_SUCCESS)
    {
        necro_print_error(&lexer->error, input_string, "Syntax");
        return;
    }
    necro_stop_and_report_timer(timer, "lexing");
    if (compilation_phase == NECRO_PHASE_LEX)
        necro_print_lexer(lexer);
    if (compilation_phase == NECRO_PHASE_LEX)
        return;

    //=====================================================
    // Parsing
    //=====================================================
    if (compilation_phase != NECRO_PHASE_JIT)
        necro_announce_phase("Parsing");
    necro_start_timer(timer);
    *ast = (NecroAST) { construct_arena(lexer->tokens.length * sizeof(NecroAST_Node)) };
    construct_parser(parser, ast, lexer->tokens.data, &lexer->intern);
    *destruct_flags |= BIT(NECRO_PHASE_PARSE);
    NecroAST_LocalPtr root_machine_ptr = null_local_ptr;
    if (parse_ast(parser, &root_machine_ptr) != NECRO_SUCCESS)
    {
        necro_print_error(&parser->error, input_string, "Parsing");
        return;
    }
    necro_stop_and_report_timer(timer, "parsing");
    if (compilation_phase == NECRO_PHASE_PARSE)
        print_ast(ast, &lexer->intern, root_machine_ptr);
    if (compilation_phase == NECRO_PHASE_PARSE)
        return;

    //=====================================================
    // Reifying
    //=====================================================
    if (compilation_phase != NECRO_PHASE_JIT)
        necro_announce_phase("Reifying");
    necro_start_timer(timer);
    *ast_r = necro_reify_ast(ast, root_machine_ptr, &lexer->intern);
    necro_stop_and_report_timer(timer, "reifying");
    if (compilation_phase == NECRO_PHASE_REIFY)
        necro_print_reified_ast(ast_r, &lexer->intern);
    *destruct_flags |= BIT(NECRO_PHASE_REIFY);
    if (compilation_phase == NECRO_PHASE_REIFY)
        return;

    //=====================================================
    // Build Scopes
    //=====================================================
    if (compilation_phase != NECRO_PHASE_JIT)
        necro_announce_phase("Scoping");
    necro_start_timer(timer);
    NecroPrimTypes      prim_types      = necro_create_prim_types(&lexer->intern);
    NecroSymTable       symtable        = necro_create_symtable(&lexer->intern);
    NecroScopedSymTable scoped_symtable = necro_create_scoped_symtable(&symtable);
    necro_init_prim_defs(&prim_types, &lexer->intern);
    *destruct_flags |= BIT(NECRO_PHASE_BUILD_SCOPES);
    if (necro_prim_build_scope(&prim_types, &scoped_symtable) != NECRO_SUCCESS)
    {
        necro_print_error(&scoped_symtable.error, input_string, "Building Prim Scopes");
        return;
    }
    if (necro_build_scopes(&scoped_symtable, ast_r) != NECRO_SUCCESS)
    {
        necro_print_error(&scoped_symtable.error, input_string, "Building Scopes");
        return;
    }
    necro_stop_and_report_timer(timer, "scoping");
    if (compilation_phase == NECRO_PHASE_BUILD_SCOPES)
    {
        necro_symtable_print(&symtable);
        necro_scoped_symtable_print(&scoped_symtable);
    }
    if (compilation_phase == NECRO_PHASE_BUILD_SCOPES)
        return;

    //=====================================================
    // Renaming
    //=====================================================
    if (compilation_phase != NECRO_PHASE_JIT)
        necro_announce_phase("Renaming");
    necro_start_timer(timer);
    NecroRenamer renamer = necro_create_renamer(&scoped_symtable, &lexer->intern);
    *destruct_flags |= BIT(NECRO_PHASE_RENAME);
    if (necro_prim_rename(&prim_types, &renamer) != NECRO_SUCCESS)
    {
        necro_print_reified_ast(ast_r, &lexer->intern);
        necro_print_error(&renamer.error, input_string, "Renaming (Prim Pass)");
        return;
    }
    if (necro_rename_declare_pass(&renamer, &ast_r->arena, ast_r->root) != NECRO_SUCCESS)
    {
        necro_print_reified_ast(ast_r, &lexer->intern);
        necro_print_error(&renamer.error, input_string, "Renaming (Declare Pass)");
        return;
    }
    if (necro_rename_var_pass(&renamer, &ast_r->arena, ast_r->root) != NECRO_SUCCESS)
    {
        necro_print_reified_ast(ast_r, &lexer->intern);
        necro_print_error(&renamer.error, input_string, "Renaming (Var Pass)");
        return;
    }
    necro_stop_and_report_timer(timer, "renaming");
    if (compilation_phase == NECRO_PHASE_RENAME)
    {
        necro_print_reified_ast(ast_r, &lexer->intern);
        return;
    }

    //=====================================================
    // Dependency Analyzing
    //=====================================================
    if (compilation_phase != NECRO_PHASE_JIT)
        necro_announce_phase("Dependency Analysis");
    NecroDependencyAnalyzer d_analyzer = necro_create_dependency_analyzer(&symtable, &lexer->intern);
    *infer = necro_create_infer(&lexer->intern, &symtable, &scoped_symtable, &renamer, &prim_types);
    necro_start_timer(timer);
    *destruct_flags |= BIT(NECRO_PHASE_DEPENDENCY_ANALYSIS);
    if (necro_prim_infer(&prim_types, &d_analyzer, infer, compilation_phase) != NECRO_SUCCESS)
    {
        necro_print_error(&infer->error, input_string, "Prim Type");
        return;
    }
    if (necro_dependency_analyze_ast(&d_analyzer, &ast_r->arena, ast_r->root))
    {
        necro_print_error(&renamer.error, input_string, "Dependency Analysis");
        return;
    }
    necro_stop_and_report_timer(timer, "d_analyze");
    if (compilation_phase == NECRO_PHASE_DEPENDENCY_ANALYSIS)
    {
        necro_print_reified_ast(ast_r, &lexer->intern);
        return;
    }

    //=====================================================
    // Infer
    //=====================================================
    if (compilation_phase != NECRO_PHASE_JIT)
        necro_announce_phase("Infer");
    necro_start_timer(timer);
    necro_infer(infer, ast_r->root);
    if (compilation_phase == NECRO_PHASE_INFER)
    {
        necro_symtable_print(&symtable);
        necro_print_type_classes(infer);
        necro_print_env_with_symtable(&symtable, infer);
    }
    *destruct_flags |= BIT(NECRO_PHASE_INFER);
    if (infer->error.return_code != NECRO_SUCCESS)
    {
        necro_print_reified_ast(ast_r, &lexer->intern);
        necro_print_error(&infer->error, input_string, "Type");
        return;
    }
    necro_type_class_translate(infer, ast_r->root);
    if (infer->error.return_code != NECRO_SUCCESS)
    {
        necro_print_reified_ast(ast_r, &lexer->intern);
        necro_print_error(&infer->error, input_string, "Type");
        return;
    }
    necro_stop_and_report_timer(timer, "infer");
    if (compilation_phase == NECRO_PHASE_INFER)
        necro_print_reified_ast(ast_r, &lexer->intern);
    if (compilation_phase == NECRO_PHASE_INFER)
        return;

    //=====================================================
    // Transform to Core
    //=====================================================
    if (compilation_phase != NECRO_PHASE_JIT)
        necro_announce_phase("Core");
    necro_start_timer(timer);
    ast_core->root = NULL;
    necro_construct_core_transform(core_transform, ast_core, ast_r, &lexer->intern, &prim_types, &symtable);
    *destruct_flags |= BIT(NECRO_PHASE_TRANSFORM_TO_CORE);
    necro_transform_to_core(core_transform);
    necro_stop_and_report_timer(timer, "core");
    if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
    {
        printf("Failed to transform to core.");
        //necro_print_error(&parser.error, input_string, "Transforming to Core");
        return;
    }
    if (compilation_phase == NECRO_PHASE_TRANSFORM_TO_CORE)
    {
        necro_print_core(ast_core, &lexer->intern);
        return;
    }

    //=====================================================
    // Closure Conversion
    //=====================================================
    if (compilation_phase != NECRO_PHASE_JIT)
        necro_announce_phase("Closure Conversion");
    necro_start_timer(timer);
    NecroCoreAST cc_core = necro_closure_conversion(ast_core, &lexer->intern, &symtable, &scoped_symtable, &prim_types, infer);
    necro_stop_and_report_timer(timer, "closure_conversion");
    if (compilation_phase == NECRO_PHASE_CLOSURE_CONVERSION)
    {
        necro_core_pretty_print(&cc_core, &symtable);
        // necro_print_core(&cc_core, &lexer->intern);
        return;
    }

    //=====================================================
    // Transform to Machine
    //=====================================================
    if (compilation_phase != NECRO_PHASE_JIT)
        necro_announce_phase("Machine");
    necro_start_timer(timer);
    *machine = necro_core_to_machine(&cc_core, &symtable, &scoped_symtable, &prim_types, infer);
    necro_stop_and_report_timer(timer, "machine");
    *destruct_flags |= BIT(NECRO_PHASE_TRANSFORM_TO_MACHINE);
    if (compilation_phase == NECRO_PHASE_TRANSFORM_TO_MACHINE)
    {
        puts("");
        necro_print_machine_program(machine);
        return;
    }

    //=====================================================
    // Codegen
    //=====================================================
    if (compilation_phase != NECRO_PHASE_JIT)
        necro_announce_phase("CodeGen");
    necro_start_timer(timer);
    *codegen_llvm = necro_create_codegen_llvm(&lexer->intern, &symtable, &prim_types, should_optimize);
    *destruct_flags |= BIT(NECRO_PHASE_CODEGEN);
    if (necro_codegen_llvm(codegen_llvm, machine) != NECRO_SUCCESS)
        return;
    necro_stop_and_report_timer(timer, "codegen");
    if (compilation_phase == NECRO_PHASE_CODEGEN)
    {
        necro_print_machine_program(machine);
        necro_print_codegen_llvm(codegen_llvm);
        return;
    }

    //=====================================================
    // JIT
    //=====================================================
    if (necro_jit_llvm(codegen_llvm) == NECRO_ERROR)
        return;
    if (compilation_phase == NECRO_PHASE_JIT)
        return;
}

bool validate_destruct_phase(NECRO_PHASE requested_phase, uint32_t destruct_flags)
{
    return (destruct_flags & BIT(requested_phase)) != 0;
}

void necro_compile_go(const char* input_string, NECRO_PHASE compilation_phase, bool should_optimize)
{
    uint32_t destruct_flags = 0;
    NecroInfer infer;
    NecroParser parser;
    NecroTransformToCore core_transform;
    NecroLexer lexer;
    NecroAST ast;
    NecroAST_Reified ast_r;
    NecroCoreAST ast_core;
    NecroMachineProgram machine;
    NecroCodeGenLLVM codegen_llvm;

    necro_compile_impl(
        input_string,
        should_optimize,
        compilation_phase,
        &infer,
        &parser,
        &core_transform,
        &lexer,
        &codegen_llvm,
        &ast,
        &ast_r,
        // &runtime,
        &ast_core,
        &machine,
        &destruct_flags);

    //=====================================================
    // Cleaning up
    //=====================================================
    necro_announce_phase("Cleaning Up");
    if (validate_destruct_phase(NECRO_PHASE_CODEGEN, destruct_flags))
    {
        necro_destroy_codegen_llvm(&codegen_llvm);
        // necro_destroy_runtime(&runtime);
    }

    if (validate_destruct_phase(NECRO_PHASE_TRANSFORM_TO_MACHINE, destruct_flags))
    {
        necro_destroy_machine_program(&machine);
    }

    if (validate_destruct_phase(NECRO_PHASE_TRANSFORM_TO_CORE, destruct_flags))
    {
        necro_destruct_core_transform(&core_transform);
    }

    if (validate_destruct_phase(NECRO_PHASE_DEPENDENCY_ANALYSIS, destruct_flags))
    {
        necro_destroy_infer(&infer);
    }

    if (validate_destruct_phase(NECRO_PHASE_PARSE, destruct_flags))
    {
        destruct_parser(&parser);
    }

    if (validate_destruct_phase(NECRO_PHASE_LEX, destruct_flags))
    {
        necro_destroy_lexer(&lexer);
    }

    if (validate_destruct_phase(NECRO_PHASE_PARSE, destruct_flags))
    {
        destruct_arena(&ast.arena);
    }
}

void necro_test(NECRO_TEST test)
{
    switch (test)
    {
    // case NECRO_TEST_VM:                necro_test_vm();                break;
    // case NECRO_TEST_DVM:               necro_test_dvm();               break;
    case NECRO_TEST_SYMTABLE:          necro_symtable_test();          break;
    // case NECRO_TEST_SLAB:              necro_test_slab();              break;
    // case NECRO_TEST_TREADMILL:         necro_test_treadmill();         break;
    case NECRO_TEST_LEXER:             necro_test_lexer();             break;
    case NECRO_TEST_INTERN:            necro_test_intern();            break;
    case NECRO_TEST_VAULT:             necro_vault_test();             break;
    case NECRO_TEST_ARCHIVE:           necro_archive_test();           break;
    case NECRO_TEST_REGION:            necro_region_test();            break;
    case NECRO_TEST_INFER:             necro_test_infer();             break;
    case NECRO_TEST_TYPE:              necro_test_type();              break;
    case NECRO_TEST_ARENA_CHAIN_TABLE: necro_arena_chain_table_test(); break;
    case NECRO_TEST_ALL:
        // necro_test_dvm();
        necro_symtable_test();
        necro_test_lexer();
        necro_test_intern();
        necro_archive_test();
        necro_region_test();
        necro_test_infer();
        necro_test_type();
        necro_arena_chain_table_test();
        break;
    default: break;
    }
}

void necro_compile_opt(const char* input_string, NECRO_PHASE compilation_phase)
{
    necro_compile_go(input_string, compilation_phase, true);
}

void necro_compile(const char* input_string, NECRO_PHASE compilation_phase)
{
    necro_compile_go(input_string, compilation_phase, false);
}

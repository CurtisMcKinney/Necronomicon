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
#include "core/lambda_lift.h"
#include "core/state_analysis.h"
#include "machine/machine.h"
#include "machine/machine_print.h"
#include "codegen/codegen_llvm.h"
#include "core/core_pretty_print.h"
#include "utility/unicode_properties.h"

#define NECRO_VERBOSITY 1

void necro_begin_phase(NecroCompileInfo compile_info, NECRO_PHASE phase, const char* phase_name)
{
    if (compile_info.compilation_phase != phase && compile_info.verbosity > 0)
    {
        necro_announce_phase(phase_name);
        necro_start_timer(compile_info.timer);
    }
}

bool necro_end_phase(NecroCompileInfo compile_info, NECRO_PHASE phase, const char* phase_name)
{
    if (compile_info.compilation_phase != phase && compile_info.verbosity > 0)
    {
        necro_stop_and_report_timer(compile_info.timer, phase_name);
    }
    return compile_info.compilation_phase == phase;
}

NecroResult(NecroUnit) necro_compile_impl(
    const char*          input_string,
    size_t               input_string_length,
    bool                 should_optimize,
    NECRO_PHASE          compilation_phase,
    NecroIntern*         intern,
    NecroLexTokenVector* lex_tokens,
    NecroInfer* infer,
    NecroParser* parser,
    NecroTransformToCore* core_transform,
    NecroCodeGenLLVM* codegen_llvm,
    NecroAST* ast,
    NecroAST_Reified* ast_r,
    NecroCoreAST* ast_core,
    NecroMachineProgram* machine,
    uint32_t* destruct_flags)
{
    if (compilation_phase == NECRO_PHASE_NONE)
        return ok_unit();

    struct NecroTimer* timer = necro_create_timer();
    NecroCompileInfo   info  = { .verbosity = NECRO_VERBOSITY, .timer = timer, .compilation_phase = compilation_phase };

    //--------------------
    // Lexing
    //--------------------
    necro_begin_phase(info, NECRO_PHASE_LEX, "Lexing"); // TODO: Phase to string
    necro_try(NecroUnit, necro_lex(input_string, input_string_length, intern, lex_tokens, info));
    if (necro_end_phase(info, NECRO_PHASE_LEX, "Lexing"))
        return ok_unit();

    //=====================================================
    // Parsing
    //=====================================================
    if (compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("Parsing");
    necro_start_timer(timer);
    *parser = construct_parser(lex_tokens->data, lex_tokens->length, intern);
    *ast    = parser->ast;
    *destruct_flags |= BIT(NECRO_PHASE_PARSE);
    NecroAST_LocalPtr root_machine_ptr = necro_try_map(NecroAST_LocalPtr, NecroUnit, parse_ast(parser));
    necro_stop_and_report_timer(timer, "parsing");
    if (compilation_phase == NECRO_PHASE_PARSE)
    {
        print_ast(ast, intern, root_machine_ptr);
        return ok_unit();
    }

    //=====================================================
    // Reifying
    //=====================================================
    if (compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("Reifying");
    necro_start_timer(timer);
    *ast_r = necro_reify_ast(ast, root_machine_ptr, intern);
    necro_stop_and_report_timer(timer, "reifying");
    *destruct_flags |= BIT(NECRO_PHASE_REIFY);
    if (compilation_phase == NECRO_PHASE_REIFY)
    {
        necro_print_reified_ast(ast_r, intern);
        return ok_unit();
    }

    //=====================================================
    // Build Scopes
    //=====================================================
    if (compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("Scoping");
    necro_start_timer(timer);
    NecroPrimTypes      prim_types      = necro_create_prim_types(intern);
    NecroSymTable       symtable        = necro_create_symtable(intern);
    NecroScopedSymTable scoped_symtable = necro_create_scoped_symtable(&symtable);
    necro_init_prim_defs(&prim_types, intern);
    *destruct_flags |= BIT(NECRO_PHASE_BUILD_SCOPES);
    if (necro_prim_build_scope(&prim_types, &scoped_symtable) != NECRO_SUCCESS)
    {
        // TODO: Error handling
        // necro_print_error(&scoped_symtable.error, input_string, "Building Prim Scopes");
        return ok_unit();
    }
    if (necro_build_scopes(&scoped_symtable, ast_r) != NECRO_SUCCESS)
    {
        // TODO: Error handling
        // necro_print_error(&scoped_symtable.error, input_string, "Building Scopes");
        return ok_unit();
    }
    necro_stop_and_report_timer(timer, "scoping");
    if (compilation_phase == NECRO_PHASE_BUILD_SCOPES)
    {
        necro_symtable_print(&symtable);
        necro_scoped_symtable_print(&scoped_symtable);
        return ok_unit();
    }

    //=====================================================
    // Renaming
    //=====================================================
    if (compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("Renaming");
    necro_start_timer(timer);
    NecroRenamer renamer = necro_create_renamer(&scoped_symtable, intern);
    *destruct_flags |= BIT(NECRO_PHASE_RENAME);
    if (necro_prim_rename(&prim_types, &renamer) != NECRO_SUCCESS)
    {
        // TODO: Error handling
        necro_print_reified_ast(ast_r, intern);
        // necro_print_error(&renamer.error, input_string, "Renaming (Prim Pass)");
        return ok_unit();
    }
    if (necro_rename_declare_pass(&renamer, &ast_r->arena, ast_r->root) != NECRO_SUCCESS)
    {
        // TODO: Error handling
        necro_print_reified_ast(ast_r, intern);
        // necro_print_error(&renamer.error, input_string, "Renaming (Declare Pass)");
        return ok_unit();
    }
    if (necro_rename_var_pass(&renamer, &ast_r->arena, ast_r->root) != NECRO_SUCCESS)
    {
        // TODO: Error handling
        necro_print_reified_ast(ast_r, intern);
        // necro_print_error(&renamer.error, input_string, "Renaming (Var Pass)");
        return ok_unit();
    }
    necro_stop_and_report_timer(timer, "renaming");
    if (compilation_phase == NECRO_PHASE_RENAME)
    {
        necro_print_reified_ast(ast_r, intern);
        return ok_unit();
    }

    //=====================================================
    // Dependency Analyzing
    //=====================================================
    if (compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("Dependency Analysis");
    NecroDependencyAnalyzer d_analyzer = necro_create_dependency_analyzer(&symtable, intern);
    *infer = necro_create_infer(intern, &symtable, &scoped_symtable, &renamer, &prim_types);
    necro_start_timer(timer);
    *destruct_flags |= BIT(NECRO_PHASE_DEPENDENCY_ANALYSIS);
    if (necro_prim_infer(&prim_types, &d_analyzer, infer, compilation_phase) != NECRO_SUCCESS)
    {
        // necro_print_error(&infer->error, input_string, "Prim Type");
        return ok_unit();
    }
    if (necro_dependency_analyze_ast(&d_analyzer, &ast_r->arena, ast_r->root))
    {
        // TODO: Error handling
        // necro_print_error(&renamer.error, input_string, "Dependency Analysis");
        return ok_unit();
    }
    necro_stop_and_report_timer(timer, "d_analyze");
    if (compilation_phase == NECRO_PHASE_DEPENDENCY_ANALYSIS)
    {
        necro_print_reified_ast(ast_r, intern);
        return ok_unit();
    }

    //=====================================================
    // Infer
    //=====================================================
    if (compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("Infer");
    necro_start_timer(timer);
    necro_infer(infer, ast_r->root);
    if (compilation_phase == NECRO_PHASE_INFER)
    {
        // necro_symtable_print(&symtable);
        // necro_print_type_classes(infer);
        necro_print_env_with_symtable(&symtable, infer);
    }
    *destruct_flags |= BIT(NECRO_PHASE_INFER);
    if (infer->error.return_code != NECRO_SUCCESS)
    {
        necro_print_reified_ast(ast_r, intern);
        // TODO: Error handling
        // necro_print_error(&infer->error, input_string, "Type");
        return ok_unit();
    }
    necro_type_class_translate(infer, ast_r->root);
    if (infer->error.return_code != NECRO_SUCCESS)
    {
        necro_print_reified_ast(ast_r, intern);
        // TODO: Error handling
        // necro_print_error(&infer->error, input_string, "Type");
        return ok_unit();
    }
    necro_stop_and_report_timer(timer, "infer");
    if (compilation_phase == NECRO_PHASE_INFER)
    {
        necro_print_reified_ast(ast_r, intern);
        return ok_unit();
    }

    //=====================================================
    // Transform to Core
    //=====================================================
    if (compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("Core");
    necro_start_timer(timer);
    ast_core->root = NULL;
    necro_construct_core_transform(core_transform, ast_core, ast_r, intern, &prim_types, &symtable, &scoped_symtable);
    *destruct_flags |= BIT(NECRO_PHASE_TRANSFORM_TO_CORE);
    necro_transform_to_core(core_transform);
    necro_stop_and_report_timer(timer, "core");
    if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
    {
        printf("Failed to transform to core.");
        //necro_print_error(&parser.error, input_string, "Transforming to Core");
        return ok_unit();
    }
    if (compilation_phase == NECRO_PHASE_TRANSFORM_TO_CORE)
    {
        necro_print_core(ast_core, intern);
        return ok_unit();
    }

    //=====================================================
    // Lambda Lift
    //=====================================================
    if (compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("Lambda Lift");
    necro_start_timer(timer);
    NecroCoreAST ll_core = necro_lambda_lift(ast_core, intern, &symtable, &scoped_symtable, &prim_types, infer);
    necro_stop_and_report_timer(timer, "lambda_lift");
    if (compilation_phase == NECRO_PHASE_LAMBDA_LIFT)
    {
        necro_core_pretty_print(&ll_core, &symtable);
        // necro_print_core(&cc_core, intern);
        return ok_unit();
    }

    //=====================================================
    // Closure Conversion
    //=====================================================
    if (compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("Closure Conversion");
    necro_start_timer(timer);
    NecroClosureDefVector closure_defs;
    NecroCoreAST          cc_core = necro_closure_conversion(&ll_core, intern, &symtable, &scoped_symtable, &prim_types, infer, &closure_defs);
    necro_stop_and_report_timer(timer, "closure_conversion");
    if (compilation_phase == NECRO_PHASE_CLOSURE_CONVERSION)
    {
        necro_core_pretty_print(&cc_core, &symtable);
        // necro_print_core(&cc_core, intern);
        return ok_unit();
    }

    //=====================================================
    // State Analysis
    //=====================================================
    if (compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("State Analysis");
    necro_start_timer(timer);
    necro_state_analysis(&cc_core, intern, &symtable, &scoped_symtable, &prim_types, infer);
    necro_stop_and_report_timer(timer, "state_analysis");
    if (compilation_phase == NECRO_PHASE_STATE_ANALYSIS)
    {
        necro_core_pretty_print(&cc_core, &symtable);
        // necro_print_core(&cc_core, intern);
        return ok_unit();
    }

    //=====================================================
    // Transform to Machine
    //=====================================================
    if (compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("Machine");
    necro_start_timer(timer);
    *machine = necro_core_to_machine(&cc_core, &symtable, &scoped_symtable, &prim_types, infer, closure_defs);
    necro_stop_and_report_timer(timer, "machine");
    *destruct_flags |= BIT(NECRO_PHASE_TRANSFORM_TO_MACHINE);
    if (compilation_phase == NECRO_PHASE_TRANSFORM_TO_MACHINE)
    {
        puts("");
        necro_print_machine_program(machine);
        return ok_unit();
    }

    //=====================================================
    // Codegen
    //=====================================================
    if (compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("CodeGen");
    necro_start_timer(timer);
    *codegen_llvm = necro_create_codegen_llvm(intern, &symtable, &prim_types, should_optimize);
    *destruct_flags |= BIT(NECRO_PHASE_CODEGEN);
    if (necro_codegen_llvm(codegen_llvm, machine) != NECRO_SUCCESS)
    {
        // TODO: Error handling
        return ok_unit();
    }
    necro_stop_and_report_timer(timer, "codegen");
    if (compilation_phase == NECRO_PHASE_CODEGEN)
    {
        necro_print_machine_program(machine);
        necro_print_codegen_llvm(codegen_llvm);
        return ok_unit();
    }

    //=====================================================
    // JIT
    //=====================================================
    if (necro_jit_llvm(codegen_llvm) == NECRO_ERROR)
    {
        // TODO: Error handling
        return ok_unit();
    }
    if (compilation_phase == NECRO_PHASE_JIT)
    {
        return ok_unit();
    }

    // TODO: Error handling
    return ok_unit();
}

bool validate_destruct_phase(NECRO_PHASE requested_phase, uint32_t destruct_flags)
{
    return (destruct_flags & BIT(requested_phase)) != 0;
}

void necro_compile_go(const char* file_name, const char* input_string, size_t input_string_length, NECRO_PHASE compilation_phase, bool should_optimize)
{
    // Persistent data
    uint32_t             destruct_flags = 0;
    NecroIntern          intern         = necro_empty_intern();
    NecroLexTokenVector  lex_tokens     = necro_empty_lex_token_vector();
    NecroInfer           infer;
    NecroParser          parser;
    NecroTransformToCore core_transform;
    NecroAST             ast;
    NecroAST_Reified     ast_r;
    NecroCoreAST         ast_core;
    NecroMachineProgram  machine;
    NecroCodeGenLLVM     codegen_llvm;

    NecroResult(NecroUnit) result = necro_compile_impl(
        input_string,
        input_string_length,
        should_optimize,
        compilation_phase,
        &intern,
        &lex_tokens,
        &infer,
        &parser,
        &core_transform,
        &codegen_llvm,
        &ast,
        &ast_r,
        // &runtime,
        &ast_core,
        &machine,
        &destruct_flags);

    if (result.type != NECRO_RESULT_OK )
    {
        necro_print_result_error(result.error, input_string, file_name);
    }

    //=====================================================
    // Cleaning up
    //=====================================================
    // necro_announce_phase("Cleaning Up");
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

    necro_destroy_intern(&intern);
    necro_destroy_lex_token_vector(&lex_tokens);

    // if (validate_destruct_phase(NECRO_PHASE_LEX, destruct_flags))
    // {
    //     necro_destroy_lexer(&lexer);
    // }

    // if (validate_destruct_phase(NECRO_PHASE_PARSE, destruct_flags))
    // {
    //     destruct_arena(&ast.arena);
    // }
}

void necro_test(NECRO_TEST test)
{
    switch (test)
    {
    case NECRO_TEST_UNICODE:           necro_test_unicode_properties(); break;
    case NECRO_TEST_SYMTABLE:          necro_symtable_test();           break;
    case NECRO_TEST_LEXER:             necro_test_lexer();              break;
    case NECRO_TEST_INTERN:            necro_test_intern();             break;
    case NECRO_TEST_INFER:             necro_test_infer();              break;
    case NECRO_TEST_TYPE:              necro_test_type();               break;
    case NECRO_TEST_ARENA_CHAIN_TABLE: necro_arena_chain_table_test();  break;
    case NECRO_TEST_ALL:
        necro_test_unicode_properties();
        // necro_symtable_test();
        necro_test_lexer();
        // necro_test_intern();
        // necro_test_infer();
        // necro_test_type();
        // necro_arena_chain_table_test();
        break;
    default: break;
    }
}

void necro_compile_opt(const char* file_name, const char* input_string, size_t input_string_length, NECRO_PHASE compilation_phase)
{
    necro_compile_go(file_name, input_string, input_string_length, compilation_phase, true);
}

void necro_compile(const char* file_name, const char* input_string, size_t input_string_length, NECRO_PHASE compilation_phase)
{
    necro_compile_go(file_name, input_string, input_string_length, compilation_phase, false);
}

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

const char* necro_phase_string(NECRO_PHASE phase)
{
    switch (phase)
    {
    case NECRO_PHASE_NONE:                 return "None";
    case NECRO_PHASE_ALL:                  return "All";
    case NECRO_PHASE_LEX:                  return "Lex";
    case NECRO_PHASE_PARSE:                return "Parse";
    case NECRO_PHASE_REIFY:                return "Reify";
    case NECRO_PHASE_BUILD_SCOPES:         return "Scope";
    case NECRO_PHASE_RENAME:               return "Rename";
    case NECRO_PHASE_DEPENDENCY_ANALYSIS:  return "DependencyAnalysis";
    case NECRO_PHASE_INFER:                return "Infer";
    case NECRO_PHASE_TRANSFORM_TO_CORE:    return "Core";
    case NECRO_PHASE_LAMBDA_LIFT:          return "LambdaLift";
    case NECRO_PHASE_CLOSURE_CONVERSION:   return "ClosureConversion";
    case NECRO_PHASE_STATE_ANALYSIS:       return "StateAnalysis";
    case NECRO_PHASE_TRANSFORM_TO_MACHINE: return "NecroMachine";
    case NECRO_PHASE_CODEGEN:              return "CodeGen";
    case NECRO_PHASE_JIT:                  return "JIT";
    default:
        assert(false);
        return NULL;
    }
}

void necro_begin_phase(NecroCompileInfo compile_info, NECRO_PHASE phase)
{
    // if (compile_info.compilation_phase == phase && compile_info.verbosity > 0)
    //     necro_announce_phase(necro_phase_string(phase));
    UNUSED(phase);
    if (compile_info.compilation_phase != NECRO_PHASE_JIT && compile_info.verbosity > 1)
    {
        necro_start_timer(compile_info.timer);
    }
}

bool necro_end_phase(NecroCompileInfo compile_info, NECRO_PHASE phase)
{
    if (compile_info.compilation_phase != NECRO_PHASE_JIT && compile_info.verbosity > 1)
    {
        necro_stop_and_report_timer(compile_info.timer, necro_phase_string(phase));
    }
    return compile_info.compilation_phase == phase;
}

NecroResult(void) necro_compile_impl(
    NecroCompileInfo      info,
    const char*           input_string,
    size_t                input_string_length,
    NecroIntern*          intern,
    NecroPrimTypes*       prim_types,
    NecroSymTable*        symtable,
    NecroScopedSymTable*  scoped_symtable,
    NecroLexTokenVector*  lex_tokens,
    NecroAST*             ast,
    NecroInfer*           infer,
    NecroTransformToCore* core_transform,
    NecroCodeGenLLVM*     codegen_llvm,
    NecroAST_Reified*     ast_r,
    NecroCoreAST*         ast_core,
    NecroMachineProgram*  machine)
{
    if (info.compilation_phase == NECRO_PHASE_NONE)
        return ok_void();

    //--------------------
    // Lexing
    //--------------------
    necro_begin_phase(info, NECRO_PHASE_LEX);
    necro_try(void, necro_lex(input_string, input_string_length, intern, lex_tokens, info));
    if (necro_end_phase(info, NECRO_PHASE_LEX))
        return ok_void();

    //--------------------
    // Parsing
    //--------------------
    necro_begin_phase(info, NECRO_PHASE_PARSE);
    necro_try(void, necro_parse(lex_tokens, intern, ast, info));
    if (necro_end_phase(info, NECRO_PHASE_PARSE))
        return ok_void();

    //=====================================================
    // Reifying
    //=====================================================
    if (info.compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("Reifying");
    // necro_start_timer(timer);
    *ast_r = necro_reify_ast(ast, ast->root, intern);
    // necro_stop_and_report_timer(timer, "reifying");
    if (info.compilation_phase == NECRO_PHASE_REIFY)
    {
        necro_print_reified_ast(ast_r, intern);
        return ok_void();
    }

    //=====================================================
    // Build Scopes
    //=====================================================
    if (info.compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("Scoping");
    // *prim_types      = necro_create_prim_types();
    // *symtable        = necro_create_symtable(intern);
    // *scoped_symtable = necro_create_scoped_symtable(symtable);
    necro_init_prim_defs(prim_types, intern);
    if (necro_prim_build_scope(prim_types, scoped_symtable) != NECRO_SUCCESS)
    {
        // TODO: Error handling
        // necro_print_error(scoped_symtable.error, input_string, "Building Prim Scopes");
        return ok_void();
    }
    if (necro_build_scopes(scoped_symtable, ast_r) != NECRO_SUCCESS)
    {
        // TODO: Error handling
        // necro_print_error(scoped_symtable.error, input_string, "Building Scopes");
        return ok_void();
    }
    // necro_stop_and_report_timer(timer, "scoping");
    if (info.compilation_phase == NECRO_PHASE_BUILD_SCOPES)
    {
        necro_symtable_print(symtable);
        necro_scoped_symtable_print(scoped_symtable);
        return ok_void();
    }

    //=====================================================
    // Renaming
    //=====================================================
    if (info.compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("Renaming");
    // necro_start_timer(timer);
    NecroRenamer renamer = necro_create_renamer(scoped_symtable, intern);
    if (necro_prim_rename(prim_types, &renamer) != NECRO_SUCCESS)
    {
        // TODO: Error handling
        necro_print_reified_ast(ast_r, intern);
        // necro_print_error(&renamer.error, input_string, "Renaming (Prim Pass)");
        return ok_void();
    }
    if (necro_rename_declare_pass(&renamer, &ast_r->arena, ast_r->root) != NECRO_SUCCESS)
    {
        // TODO: Error handling
        necro_print_reified_ast(ast_r, intern);
        // necro_print_error(&renamer.error, input_string, "Renaming (Declare Pass)");
        return ok_void();
    }
    if (necro_rename_var_pass(&renamer, &ast_r->arena, ast_r->root) != NECRO_SUCCESS)
    {
        // TODO: Error handling
        necro_print_reified_ast(ast_r, intern);
        // necro_print_error(&renamer.error, input_string, "Renaming (Var Pass)");
        return ok_void();
    }
    // necro_stop_and_report_timer(timer, "renaming");
    if (info.compilation_phase == NECRO_PHASE_RENAME)
    {
        necro_print_reified_ast(ast_r, intern);
        return ok_void();
    }

    //=====================================================
    // Dependency Analyzing
    //=====================================================
    if (info.compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("Dependency Analysis");
    NecroDependencyAnalyzer d_analyzer = necro_create_dependency_analyzer(symtable, intern);
    *infer = necro_create_infer(intern, symtable, scoped_symtable, &renamer, prim_types);
    // necro_start_timer(timer);
    if (necro_prim_infer(prim_types, &d_analyzer, infer, info.compilation_phase) != NECRO_SUCCESS)
    {
        // necro_print_error(&infer->error, input_string, "Prim Type");
        return ok_void();
    }
    if (necro_dependency_analyze_ast(&d_analyzer, &ast_r->arena, ast_r->root))
    {
        // TODO: Error handling
        // necro_print_error(&renamer.error, input_string, "Dependency Analysis");
        return ok_void();
    }
    // necro_stop_and_report_timer(timer, "d_analyze");
    if (info.compilation_phase == NECRO_PHASE_DEPENDENCY_ANALYSIS)
    {
        necro_print_reified_ast(ast_r, intern);
        return ok_void();
    }

    //=====================================================
    // Infer
    //=====================================================
    if (info.compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("Infer");
    // necro_start_timer(timer);
    necro_infer(infer, ast_r->root);
    if (info.compilation_phase == NECRO_PHASE_INFER)
    {
        // necro_symtable_print(symtable);
        // necro_print_type_classes(infer);
        necro_print_env_with_symtable(symtable, infer);
    }
    if (infer->error.return_code != NECRO_SUCCESS)
    {
        necro_print_reified_ast(ast_r, intern);
        // TODO: Error handling
        // necro_print_error(&infer->error, input_string, "Type");
        return ok_void();
    }
    necro_type_class_translate(infer, ast_r->root);
    if (infer->error.return_code != NECRO_SUCCESS)
    {
        necro_print_reified_ast(ast_r, intern);
        // TODO: Error handling
        // necro_print_error(&infer->error, input_string, "Type");
        return ok_void();
    }
    // necro_stop_and_report_timer(timer, "infer");
    if (info.compilation_phase == NECRO_PHASE_INFER)
    {
        necro_print_reified_ast(ast_r, intern);
        return ok_void();
    }

    //=====================================================
    // Transform to Core
    //=====================================================
    if (info.compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("Core");
    // necro_start_timer(timer);
    ast_core->root = NULL;
    necro_construct_core_transform(core_transform, ast_core, ast_r, intern, prim_types, symtable, scoped_symtable);
    necro_transform_to_core(core_transform);
    // necro_stop_and_report_timer(timer, "core");
    if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
    {
        printf("Failed to transform to core.");
        //necro_print_error(&parser.error, input_string, "Transforming to Core");
        return ok_void();
    }
    if (info.compilation_phase == NECRO_PHASE_TRANSFORM_TO_CORE)
    {
        necro_print_core(ast_core, intern);
        return ok_void();
    }

    //=====================================================
    // Lambda Lift
    //=====================================================
    if (info.compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("Lambda Lift");
    // necro_start_timer(timer);
    NecroCoreAST ll_core = necro_lambda_lift(ast_core, intern, symtable, scoped_symtable, prim_types, infer);
    // necro_stop_and_report_timer(timer, "lambda_lift");
    if (info.compilation_phase == NECRO_PHASE_LAMBDA_LIFT)
    {
        necro_core_pretty_print(&ll_core, symtable);
        // necro_print_core(&cc_core, intern);
        return ok_void();
    }

    //=====================================================
    // Closure Conversion
    //=====================================================
    if (info.compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("Closure Conversion");
    // necro_start_timer(timer);
    NecroClosureDefVector closure_defs;
    NecroCoreAST          cc_core = necro_closure_conversion(&ll_core, intern, symtable, scoped_symtable, prim_types, infer, &closure_defs);
    // necro_stop_and_report_timer(timer, "closure_conversion");
    if (info.compilation_phase == NECRO_PHASE_CLOSURE_CONVERSION)
    {
        necro_core_pretty_print(&cc_core, symtable);
        // necro_print_core(&cc_core, intern);
        return ok_void();
    }

    //=====================================================
    // State Analysis
    //=====================================================
    if (info.compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("State Analysis");
    // necro_start_timer(timer);
    necro_state_analysis(&cc_core, intern, symtable, scoped_symtable, prim_types, infer);
    // necro_stop_and_report_timer(timer, "state_analysis");
    if (info.compilation_phase == NECRO_PHASE_STATE_ANALYSIS)
    {
        necro_core_pretty_print(&cc_core, symtable);
        // necro_print_core(&cc_core, intern);
        return ok_void();
    }

    //=====================================================
    // Transform to Machine
    //=====================================================
    if (info.compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("Machine");
    // necro_start_timer(timer);
    *machine = necro_core_to_machine(&cc_core, symtable, scoped_symtable, prim_types, infer, closure_defs);
    // necro_stop_and_report_timer(timer, "machine");
    if (info.compilation_phase == NECRO_PHASE_TRANSFORM_TO_MACHINE)
    {
        puts("");
        necro_print_machine_program(machine);
        return ok_void();
    }

    //=====================================================
    // Codegen
    //=====================================================
    if (info.compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("CodeGen");
    // necro_start_timer(timer);
    *codegen_llvm = necro_create_codegen_llvm(intern, symtable, prim_types, info.opt_level);
    if (necro_codegen_llvm(codegen_llvm, machine) != NECRO_SUCCESS)
    {
        // TODO: Error handling
        return ok_void();
    }
    // necro_stop_and_report_timer(timer, "codegen");
    if (info.compilation_phase == NECRO_PHASE_CODEGEN)
    {
        necro_print_machine_program(machine);
        necro_print_codegen_llvm(codegen_llvm);
        return ok_void();
    }

    //=====================================================
    // JIT
    //=====================================================
    if (necro_jit_llvm(codegen_llvm) == NECRO_ERROR)
    {
        // TODO: Error handling
        return ok_void();
    }
    if (info.compilation_phase == NECRO_PHASE_JIT)
    {
        return ok_void();
    }

    // TODO: Error handling
    return ok_void();
}

void necro_compile(const char* file_name, const char* input_string, size_t input_string_length, NECRO_PHASE compilation_phase, NECRO_OPT_LEVEL opt_level)
{
    //--------------------
    // Global data
    //--------------------
    NecroIntern          intern          = necro_create_intern();
    NecroPrimTypes       prim_types      = necro_create_prim_types();
    NecroSymTable        symtable        = necro_create_symtable(&intern);
    NecroScopedSymTable  scoped_symtable = necro_create_scoped_symtable(&symtable);

    //--------------------
    // Pass data
    //--------------------
    NecroLexTokenVector  lex_tokens      = necro_empty_lex_token_vector();
    NecroAST             ast             = necro_empty_ast();
    NecroInfer           infer           = necro_empty_infer();
    NecroTransformToCore core_transform  = necro_empty_core_transform();
    NecroAST_Reified     ast_r           = necro_empty_reified_ast();
    NecroCoreAST         ast_core        = necro_empty_core_ast();
    NecroMachineProgram  machine         = necro_empty_machine_program();
    NecroCodeGenLLVM     codegen_llvm    = necro_empty_codegen_llvm();

    //--------------------
    // Compile
    //--------------------
    struct NecroTimer* timer  = necro_create_timer();
    NecroCompileInfo   info   = { .verbosity = 1, .timer = timer, .compilation_phase = compilation_phase, .opt_level = opt_level };
    NecroResult(void)  result = necro_compile_impl(
        info,
        input_string,
        input_string_length,
        &intern,
        &prim_types,
        &symtable,
        &scoped_symtable,
        &lex_tokens,
        &ast,
        &infer,
        &core_transform,
        &codegen_llvm,
        &ast_r,
        &ast_core,
        &machine);

    //--------------------
    // Error Handling
    //--------------------
    if (result.type != NECRO_RESULT_OK )
    {
        necro_print_result_error(result.error, input_string, file_name);
    }

    //--------------------
    // Clean up
    //--------------------
    necro_destroy_timer(timer);

    // Pass data
    necro_destroy_codegen_llvm(&codegen_llvm);
    necro_destroy_machine_program(&machine);
    necro_destroy_core_ast(&ast_core);
    necro_destroy_reified_ast(&ast_r);
    necro_destruct_core_transform(&core_transform);
    necro_destroy_infer(&infer);
    necro_destroy_ast(&ast);
    necro_destroy_lex_token_vector(&lex_tokens);

    // Global data
    necro_destroy_scoped_symtable(&scoped_symtable);
    necro_destroy_symtable(&symtable);
    necro_destroy_prim_types(&prim_types);
    necro_destroy_intern(&intern);
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

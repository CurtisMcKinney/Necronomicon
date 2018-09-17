/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include "utility.h"
#include "lexer.h"
#include "parse/parser.h"
#include "parse/parse_test.h"
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

const char* necro_compile_phase_string(NECRO_PHASE phase)
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

void necro_compile_begin_phase(NecroCompileInfo compile_info, NECRO_PHASE phase)
{
    // if (compile_info.compilation_phase == phase && compile_info.verbosity > 0)
    //     necro_announce_phase(necro_phase_string(phase));
    UNUSED(phase);
    if (compile_info.compilation_phase != NECRO_PHASE_JIT && compile_info.verbosity > 1)
    {
        necro_timer_start(compile_info.timer);
    }
}

bool necro_compile_end_phase(NecroCompileInfo compile_info, NECRO_PHASE phase)
{
    if (compile_info.compilation_phase != NECRO_PHASE_JIT && compile_info.verbosity > 1)
    {
        necro_timer_stop_and_report(compile_info.timer, necro_compile_phase_string(phase));
    }
    return compile_info.compilation_phase == phase;
}

NecroResult(void) necro_compile_go(
    NecroCompileInfo      info,
    const char*           input_string,
    size_t                input_string_length,
    NecroIntern*          intern,
    NecroPrimTypes*       prim_types,
    NecroSymTable*        symtable,
    NecroScopedSymTable*  scoped_symtable,
    NecroLexTokenVector*  lex_tokens,
    NecroParseAstArena*   parse_ast,
    NecroAstArena*        ast,
    NecroInfer*           infer,
    NecroTransformToCore* core_transform,
    NecroCodeGenLLVM*     codegen_llvm,
    NecroCoreAST*         ast_core,
    NecroMachineProgram*  machine)
{
    if (info.compilation_phase == NECRO_PHASE_NONE)
        return ok_void();

    //--------------------
    // Lex
    //--------------------
    necro_compile_begin_phase(info, NECRO_PHASE_LEX);
    necro_try(void, necro_lex(info, intern, input_string, input_string_length, lex_tokens));
    if (necro_compile_end_phase(info, NECRO_PHASE_LEX))
        return ok_void();

    //--------------------
    // Parse
    //--------------------
    necro_compile_begin_phase(info, NECRO_PHASE_PARSE);
    necro_try(void, necro_parse(info, intern, lex_tokens, parse_ast));
    if (necro_compile_end_phase(info, NECRO_PHASE_PARSE))
        return ok_void();

    //--------------------
    // Reify
    //--------------------
    necro_compile_begin_phase(info, NECRO_PHASE_REIFY);
    *ast = necro_reify(info, intern, parse_ast);
    if (necro_compile_end_phase(info, NECRO_PHASE_REIFY))
        return ok_void();

    //--------------------
    // Build Scopes
    //--------------------
    // TODO: What to do about primitives?
    necro_compile_begin_phase(info, NECRO_PHASE_BUILD_SCOPES);
    necro_prim_types_init_prim_defs(prim_types, intern);
    necro_prim_types_build_scopes(prim_types, scoped_symtable);
    necro_build_scopes(info, scoped_symtable, ast);
    if (necro_compile_end_phase(info, NECRO_PHASE_BUILD_SCOPES))
        return ok_void();

    //--------------------
    // Rename
    //--------------------
    necro_compile_begin_phase(info, NECRO_PHASE_RENAME);
    NecroRenamer renamer = necro_create_renamer(scoped_symtable, intern);
    if (necro_prim_types_rename(prim_types, &renamer) != NECRO_SUCCESS)
    {
        // TODO: Error handling
        necro_ast_arena_print(ast);
        // necro_print_error(&renamer.error, input_string, "Renaming (Prim Pass)");
        return ok_void();
    }
    necro_try(void, necro_rename(info, scoped_symtable, intern, ast));
    if (necro_compile_end_phase(info, NECRO_PHASE_BUILD_SCOPES))
        return ok_void();

    //=====================================================
    // Dependency Analyzing
    //=====================================================
    if (info.compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("Dependency Analysis");
    NecroDependencyAnalyzer d_analyzer = necro_create_dependency_analyzer(symtable, intern);
    *infer = necro_create_infer(intern, symtable, scoped_symtable, &renamer, prim_types);
    // necro_start_timer(timer);
    if (necro_prim_types_infer(prim_types, &d_analyzer, infer, info.compilation_phase) != NECRO_SUCCESS)
    {
        // necro_print_error(&infer->error, input_string, "Prim Type");
        return ok_void();
    }
    if (necro_dependency_analyze_ast(&d_analyzer, &ast->arena, ast->root))
    {
        // TODO: Error handling
        // necro_print_error(&renamer.error, input_string, "Dependency Analysis");
        return ok_void();
    }
    // necro_stop_and_report_timer(timer, "d_analyze");
    if (info.compilation_phase == NECRO_PHASE_DEPENDENCY_ANALYSIS)
    {
        necro_ast_arena_print(ast);
        return ok_void();
    }

    //=====================================================
    // Infer
    //=====================================================
    if (info.compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("Infer");
    // necro_start_timer(timer);
    necro_infer(infer, ast->root);
    if (info.compilation_phase == NECRO_PHASE_INFER)
    {
        // necro_symtable_print(symtable);
        // necro_print_type_classes(infer);
        necro_symtable_print_env(symtable, infer);
    }
    if (infer->error.return_code != NECRO_SUCCESS)
    {
        necro_ast_arena_print(ast);
        // TODO: Error handling
        // necro_print_error(&infer->error, input_string, "Type");
        return ok_void();
    }
    necro_type_class_translate(infer, ast->root);
    if (infer->error.return_code != NECRO_SUCCESS)
    {
        necro_ast_arena_print(ast);
        // TODO: Error handling
        // necro_print_error(&infer->error, input_string, "Type");
        return ok_void();
    }
    // necro_stop_and_report_timer(timer, "infer");
    if (info.compilation_phase == NECRO_PHASE_INFER)
    {
        necro_ast_arena_print(ast);
        return ok_void();
    }

    //=====================================================
    // Transform to Core
    //=====================================================
    if (info.compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("Core");
    // necro_start_timer(timer);
    ast_core->root = NULL;
    necro_construct_core_transform(core_transform, ast_core, ast, intern, prim_types, symtable, scoped_symtable);
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
    NecroIntern          intern          = necro_intern_create();
    NecroPrimTypes       prim_types      = necro_prim_types_create();
    NecroSymTable        symtable        = necro_symtable_create(&intern);
    NecroScopedSymTable  scoped_symtable = necro_scoped_symtable_create(&symtable);

    //--------------------
    // Pass data
    //--------------------
    NecroLexTokenVector  lex_tokens      = necro_empty_lex_token_vector();
    NecroParseAstArena   parse_ast       = necro_parse_ast_arena_empty();
    NecroAstArena        ast             = necro_ast_arena_empty();
    NecroInfer           infer           = necro_empty_infer();
    NecroTransformToCore core_transform  = necro_empty_core_transform();
    NecroCoreAST         ast_core        = necro_empty_core_ast();
    NecroMachineProgram  machine         = necro_empty_machine_program();
    NecroCodeGenLLVM     codegen_llvm    = necro_empty_codegen_llvm();

    //--------------------
    // Compile
    //--------------------
    struct NecroTimer* timer  = necro_timer_create();
    NecroCompileInfo   info   = { .verbosity = 1, .timer = timer, .compilation_phase = compilation_phase, .opt_level = opt_level };
    NecroResult(void)  result = necro_compile_go(
        info,
        input_string,
        input_string_length,
        &intern,
        &prim_types,
        &symtable,
        &scoped_symtable,
        &lex_tokens,
        &parse_ast,
        &ast,
        &infer,
        &core_transform,
        &codegen_llvm,
        &ast_core,
        &machine);

    //--------------------
    // Error Handling
    //--------------------
    if (result.type != NECRO_RESULT_OK )
    {
        necro_result_error_print(result.error, input_string, file_name);
    }

    //--------------------
    // Clean up
    //--------------------
    necro_timer_destroy(timer);

    // Pass data
    necro_destroy_codegen_llvm(&codegen_llvm);
    necro_destroy_machine_program(&machine);
    necro_destroy_core_ast(&ast_core);
    necro_destruct_core_transform(&core_transform);
    necro_destroy_infer(&infer);
    necro_ast_arena_destroy(&ast);
    necro_parse_ast_arena_destroy(&parse_ast);
    necro_destroy_lex_token_vector(&lex_tokens);

    // Global data
    necro_scoped_symtable_destroy(&scoped_symtable);
    necro_symtable_destroy(&symtable);
    necro_prim_types_destroy(&prim_types);
    necro_intern_destroy(&intern);
}

void necro_test(NECRO_TEST test)
{
    switch (test)
    {
    case NECRO_TEST_UNICODE:           necro_test_unicode_properties(); break;
    case NECRO_TEST_SYMTABLE:          necro_symtable_test();           break;
    case NECRO_TEST_LEXER:             necro_lex_test();                break;
    case NECRO_TEST_PARSER:            necro_parse_test ();             break;
    case NECRO_TEST_INTERN:            necro_intern_test();             break;
    case NECRO_TEST_INFER:             necro_test_infer();              break;
    case NECRO_TEST_TYPE:              necro_test_type();               break;
    case NECRO_TEST_ARENA_CHAIN_TABLE: necro_arena_chain_table_test();  break;
    case NECRO_TEST_ALL:
        necro_test_unicode_properties();
        necro_intern_test();
        necro_lex_test();
        necro_parse_test();
        // necro_symtable_test();
        // necro_test_infer();
        // necro_test_type();
        // necro_arena_chain_table_test();
        break;
    default: break;
    }
}

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
#include "base.h"
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
#include "core/defunctionalization.h"
#include "core/state_analysis.h"
#include "machine/machine.h"
#include "machine/machine_print.h"
#include "codegen/codegen_llvm.h"
#include "core/core_pretty_print.h"
#include "utility/unicode_properties.h"
#include "type/monomorphize.h"

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
    case NECRO_PHASE_MONOMORPHIZE:         return "Monomorphize";
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
    NecroBase*            base,
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
    necro_try(void, necro_parse(info, intern, lex_tokens, necro_intern_string(intern, "Main"), parse_ast));
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
    necro_compile_begin_phase(info, NECRO_PHASE_BUILD_SCOPES);
    necro_build_scopes(info, scoped_symtable, ast);
    if (necro_compile_end_phase(info, NECRO_PHASE_BUILD_SCOPES))
        return ok_void();

    //--------------------
    // Rename
    //--------------------
    necro_compile_begin_phase(info, NECRO_PHASE_RENAME);
    necro_try(void, necro_rename(info, scoped_symtable, intern, ast));
    if (necro_compile_end_phase(info, NECRO_PHASE_RENAME))
        return ok_void();

    //--------------------
    // Dependency Analyze
    //--------------------
    necro_compile_begin_phase(info, NECRO_PHASE_DEPENDENCY_ANALYSIS);
    necro_dependency_analyze(info, intern, ast);
    if (necro_compile_end_phase(info, NECRO_PHASE_DEPENDENCY_ANALYSIS))
        return ok_void();

    //--------------------
    // Infer
    //--------------------
    necro_compile_begin_phase(info, NECRO_PHASE_INFER);
    necro_try(void, necro_infer(info, intern, scoped_symtable, base, ast));
    if (necro_compile_end_phase(info, NECRO_PHASE_INFER))
        return ok_void();

    //--------------------
    // Monomorphize
    //--------------------
    necro_compile_begin_phase(info, NECRO_PHASE_MONOMORPHIZE);
    necro_try(void, necro_monomorphize(info, intern, scoped_symtable, base, ast));
    if (necro_compile_end_phase(info, NECRO_PHASE_MONOMORPHIZE))
        return ok_void();

    //=====================================================
    // Transform to Core
    //=====================================================
    if (info.compilation_phase != NECRO_PHASE_JIT && NECRO_VERBOSITY > 0)
        necro_announce_phase("Core");
    // necro_start_timer(timer);
    ast_core->root = NULL;
    necro_construct_core_transform(core_transform, ast_core, ast, intern, NULL, symtable, scoped_symtable);
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
    NecroCoreAST ll_core = necro_lambda_lift(ast_core, intern, symtable, scoped_symtable, NULL);
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
    NecroCoreAST          cc_core = necro_closure_conversion(&ll_core, intern, symtable, scoped_symtable, base, &closure_defs);
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
    necro_state_analysis(&cc_core, intern, symtable, scoped_symtable, NULL);
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
    *machine = necro_core_to_machine(&cc_core, symtable, scoped_symtable, NULL, infer, closure_defs);
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
    *codegen_llvm = necro_create_codegen_llvm(intern, symtable, NULL, info.opt_level);
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
    NecroSymTable        symtable        = necro_symtable_create(&intern);
    NecroScopedSymTable  scoped_symtable = necro_scoped_symtable_create(&symtable);
    NecroBase            base            = necro_base_compile(&intern, &scoped_symtable);

    //--------------------
    // Pass data
    //--------------------
    NecroLexTokenVector  lex_tokens      = necro_empty_lex_token_vector();
    NecroParseAstArena   parse_ast       = necro_parse_ast_arena_empty();
    NecroAstArena        ast             = necro_ast_arena_empty();
    NecroInfer           infer           = necro_infer_empty();
    NecroTransformToCore core_transform  = necro_empty_core_transform();
    NecroCoreAST         ast_core        = necro_empty_core_ast();
    NecroMachineProgram  machine         = necro_empty_machine_program();
    NecroCodeGenLLVM     codegen_llvm    = necro_empty_codegen_llvm();

    //--------------------
    // Compile
    //--------------------
    struct NecroTimer* timer  = necro_timer_create();
    NecroCompileInfo   info   = { .verbosity = 0, .timer = timer, .compilation_phase = compilation_phase, .opt_level = opt_level };
    NecroResult(void)  result = necro_compile_go(
        info,
        input_string,
        input_string_length,
        &intern,
        &base,
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
        necro_result_error_print(result.error, input_string, file_name);
    else
        necro_result_error_destroy(result.type, result.error);

    //--------------------
    // Clean up
    //--------------------
    necro_timer_destroy(timer);

    // Pass data
    necro_destroy_codegen_llvm(&codegen_llvm);
    necro_destroy_machine_program(&machine);
    necro_destroy_core_ast(&ast_core);
    necro_destruct_core_transform(&core_transform);
    necro_infer_destroy(&infer);
    necro_ast_arena_destroy(&ast);
    necro_parse_ast_arena_destroy(&parse_ast);
    necro_destroy_lex_token_vector(&lex_tokens);

    // Global data
    necro_base_destroy(&base);
    necro_scoped_symtable_destroy(&scoped_symtable);
    necro_symtable_destroy(&symtable);
    necro_intern_destroy(&intern);
}

void necro_test(NECRO_TEST test)
{
    switch (test)
    {
    case NECRO_TEST_UNICODE:              necro_test_unicode_properties();   break;
    case NECRO_TEST_LEXER:                necro_lex_test();                  break;
    case NECRO_TEST_PARSER:               necro_parse_test ();               break;
    case NECRO_TEST_INTERN:               necro_intern_test();               break;
    case NECRO_TEST_RENAME:               necro_rename_test();               break;
    case NECRO_TEST_INFER:                necro_test_infer();                break;
    case NECRO_TEST_MONOMORPHIZE:         necro_monomorphize_test();         break;
    case NECRO_TEST_ARENA_CHAIN_TABLE:    necro_arena_chain_table_test();    break;
    case NECRO_TEST_BASE:                 necro_base_test();                 break;
    case NECRO_TEST_ALL:
        necro_test_unicode_properties();
        necro_intern_test();
        necro_lex_test();
        necro_parse_test();
        necro_rename_test();
        necro_base_test();
        necro_test_infer();
        necro_monomorphize_test();
        // necro_arena_chain_table_test();
        break;
    default: break;
    }
}

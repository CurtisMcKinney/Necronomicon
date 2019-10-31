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
#include "type/type_class.h"
#include "utility/hash_table.h"
#include "d_analyzer.h"
#include "driver.h"
#include "core/core_simplify.h"
#include "core/lambda_lift.h"
#include "core/defunctionalization.h"
#include "core/state_analysis.h"
#include "utility/result.h"
#include "utility/unicode_properties.h"
#include "type/monomorphize.h"
#include "alias_analysis.h"
#include "core_ast.h"
#include "defunctionalization.h"
#include "mach_transform.h"
#include "codegen/codegen_llvm.h"

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
    case NECRO_PHASE_PRE_SIMPLIFY:         return "PreSimplify";
    case NECRO_PHASE_LAMBDA_LIFT:          return "LambdaLift";
    case NECRO_PHASE_DEFUNCTIONALIZATION:  return "Defunctionalization";
    case NECRO_PHASE_STATE_ANALYSIS:       return "StateAnalysis";
    case NECRO_PHASE_TRANSFORM_TO_MACHINE: return "NecroMachine";
    case NECRO_PHASE_CODEGEN:              return "CodeGen";
    case NECRO_PHASE_JIT:                  return "JIT";
    case NECRO_PHASE_COMPILE:              return "Compile";
    default:
        assert(false);
        return NULL;
    }
}

void necro_compile_begin_phase(NecroCompileInfo compile_info, NECRO_PHASE phase)
{
    // if (info.verbosity > 1 || (compile_info.compilation_phase == phase && compile_info.verbosity > 0))
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
    NecroScopedSymTable*  scoped_symtable,
    NecroLexTokenVector*  lex_tokens,
    NecroParseAstArena*   parse_ast,
    NecroAstArena*        ast,
    NecroCoreAstArena*    core_ast_arena,
    NecroMachProgram*     mach_program,
    NecroLLVM*            llvm)
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
    necro_dependency_analyze(info, intern, base, ast);
    necro_alias_analysis(info, ast); // NOTE: Consider merging alias_analysis into RENAME_VAR phase?
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

    //--------------------
    // Transform to Core
    //--------------------
    necro_compile_begin_phase(info, NECRO_PHASE_TRANSFORM_TO_CORE);
    necro_try(void, necro_ast_transform_to_core(info, intern, base, ast, core_ast_arena))
    if (necro_compile_end_phase(info, NECRO_PHASE_TRANSFORM_TO_CORE))
        return ok_void();

    //--------------------
    // Pre-Simplify
    //--------------------
    necro_compile_begin_phase(info, NECRO_PHASE_PRE_SIMPLIFY);
    necro_core_ast_pre_simplify(info, intern, base, core_ast_arena);
    if (necro_compile_end_phase(info, NECRO_PHASE_PRE_SIMPLIFY))
        return ok_void();

    //--------------------
    // Lambda Lift
    //--------------------
    necro_compile_begin_phase(info, NECRO_PHASE_LAMBDA_LIFT);
    necro_core_lambda_lift(info, intern, base, core_ast_arena);
    if (necro_compile_end_phase(info, NECRO_PHASE_LAMBDA_LIFT))
        return ok_void();

    //--------------------
    // Defunctionalization
    //--------------------
    necro_compile_begin_phase(info, NECRO_PHASE_DEFUNCTIONALIZATION);
    necro_core_defunctionalize(info, intern, base, core_ast_arena);
    necro_core_ast_pre_simplify(info, intern, base, core_ast_arena);
    if (necro_compile_end_phase(info, NECRO_PHASE_DEFUNCTIONALIZATION))
        return ok_void();

    //--------------------
    // StateAnalysis
    //--------------------
    necro_compile_begin_phase(info, NECRO_PHASE_STATE_ANALYSIS);
    necro_core_state_analysis(info, intern, base, core_ast_arena);
    if (necro_compile_end_phase(info, NECRO_PHASE_STATE_ANALYSIS))
        return ok_void();

    //--------------------
    // MachTransform
    //--------------------
    necro_compile_begin_phase(info, NECRO_PHASE_TRANSFORM_TO_MACHINE);
    necro_core_transform_to_mach(info, intern, base, core_ast_arena, mach_program);
    if (necro_compile_end_phase(info, NECRO_PHASE_TRANSFORM_TO_MACHINE))
        return ok_void();

    //--------------------
    // Codegen
    //--------------------
    necro_compile_begin_phase(info, NECRO_PHASE_CODEGEN);
    necro_llvm_codegen(info, mach_program, llvm);
    if (necro_compile_end_phase(info, NECRO_PHASE_CODEGEN))
        return ok_void();

    if (info.compilation_phase == NECRO_PHASE_JIT)
    {
        //--------------------
        // JIT
        //--------------------
        necro_compile_begin_phase(info, NECRO_PHASE_JIT);
        necro_llvm_jit(info, llvm);
        if (necro_compile_end_phase(info, NECRO_PHASE_JIT))
            return ok_void();
    }
    else if (info.compilation_phase == NECRO_PHASE_COMPILE)
    {
        //--------------------
        // Compile
        //--------------------
        necro_compile_begin_phase(info, NECRO_PHASE_COMPILE);
        necro_llvm_jit(info, llvm);
        if (necro_compile_end_phase(info, NECRO_PHASE_COMPILE))
            return ok_void();
    }

    return ok_void();
}

void necro_compile(const char* file_name, const char* input_string, size_t input_string_length, NECRO_PHASE compilation_phase, NECRO_OPT_LEVEL opt_level)
{
    //--------------------
    // Global data
    //--------------------
    NecroIntern          intern          = necro_intern_create();
    NecroScopedSymTable  scoped_symtable = necro_scoped_symtable_create();
    NecroBase            base            = necro_base_compile(&intern, &scoped_symtable);

    //--------------------
    // Pass data
    //--------------------
    NecroLexTokenVector  lex_tokens      = necro_empty_lex_token_vector();
    NecroParseAstArena   parse_ast       = necro_parse_ast_arena_empty();
    NecroAstArena        ast             = necro_ast_arena_empty();
    NecroCoreAstArena    core_ast_arena  = necro_core_ast_arena_empty();
    NecroMachProgram     mach_program    = necro_mach_program_empty();
    NecroLLVM            llvm            = necro_llvm_empty();

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
        &base,
        &scoped_symtable,
        &lex_tokens,
        &parse_ast,
        &ast,
        &core_ast_arena,
        &mach_program,
        &llvm);

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
    necro_llvm_destroy(&llvm);
    necro_mach_program_destroy(&mach_program);
    necro_core_ast_arena_destroy(&core_ast_arena);
    necro_ast_arena_destroy(&ast);
    necro_parse_ast_arena_destroy(&parse_ast);
    necro_destroy_lex_token_vector(&lex_tokens);

    // Global data
    necro_base_destroy(&base);
    necro_scoped_symtable_destroy(&scoped_symtable);
    necro_intern_destroy(&intern);
}

void necro_test(NECRO_TEST test)
{
    switch (test)
    {
    case NECRO_TEST_UNICODE:              necro_test_unicode_properties();    break;
    case NECRO_TEST_LEXER:                necro_lex_test();                   break;
    case NECRO_TEST_PARSER:               necro_parse_test ();                break;
    case NECRO_TEST_INTERN:               necro_intern_test();                break;
    case NECRO_TEST_RENAME:               necro_rename_test();                break;
    case NECRO_TEST_ALIAS:                necro_alias_analysis_test();        break;
    case NECRO_TEST_INFER:                necro_test_infer();                 break;
    case NECRO_TEST_MONOMORPHIZE:         necro_monomorphize_test();          break;
    case NECRO_TEST_CORE:                 necro_core_ast_test();              break;
    case NECRO_TEST_PRE_SIMPLIFY:         necro_core_ast_pre_simplify_test(); break;
    case NECRO_TEST_LAMBDA_LIFT:          necro_core_lambda_lift_test();      break;
    case NECRO_TEST_DEFUNCTIONALIZE:      necro_core_defunctionalize_test();  break;
    case NECRO_TEST_ARENA_CHAIN_TABLE:    necro_arena_chain_table_test();     break;
    case NECRO_TEST_BASE:                 necro_base_test();                  break;
    case NECRO_TEST_STATE_ANALYSIS:       necro_state_analysis_test();        break;
    case NECRO_TEST_MACH:                 necro_mach_test();                  break;
    case NECRO_TEST_LLVM:                 necro_llvm_test();                  break;
    case NECRO_TEST_JIT:                  necro_llvm_test_jit();              break;
    case NECRO_TEST_COMPILE:              necro_llvm_test_compile();          break;
    case NECRO_TEST_ALL:
        necro_test_unicode_properties();
        necro_intern_test();
        necro_lex_test();
        necro_parse_test();
        necro_rename_test();
        necro_base_test();
        necro_alias_analysis_test();
        necro_test_infer();
        necro_monomorphize_test();
        necro_core_ast_test();
        necro_core_ast_pre_simplify_test();
        necro_core_lambda_lift_test();
        necro_core_defunctionalize_test();
        necro_state_analysis_test();
        necro_mach_test();
        necro_llvm_test();
        break;
    default:
        break;
    }
}

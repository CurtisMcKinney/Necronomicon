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

void necro_compile_impl(
    const char* input_string,
    NECRO_PHASE compilation_phase,
    NecroInfer* infer,
    // NecroTypeClassEnv* type_class_env,
    NecroParser* parser,
    NecroTransformToCore* core_transform,
    NecroLexer* lexer,
    NecroAST* ast,
    uint32_t* destruct_flags)
{
    if (compilation_phase == NECRO_PHASE_NONE)
        return;

    //=====================================================
    // Lexing, PRE - Layout
    //=====================================================
    necro_announce_phase("Lexing");
    *lexer = necro_create_lexer(input_string);
    *destruct_flags |= BIT(NECRO_PHASE_LEX);
    if (necro_lex(lexer) != NECRO_SUCCESS)
    {
        necro_print_error(&lexer->error, input_string, "Syntax");
        return;
    }
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
    necro_print_lexer(lexer);
    if (compilation_phase == NECRO_PHASE_LEX)
        return;

    //=====================================================
    // Parsing
    //=====================================================
    necro_announce_phase("Parsing");
    *ast = (NecroAST) { construct_arena(lexer->tokens.length * sizeof(NecroAST_Node)) };
    construct_parser(parser, ast, lexer->tokens.data, &lexer->intern);
    *destruct_flags |= BIT(NECRO_PHASE_PARSE);
    NecroAST_LocalPtr root_node_ptr = null_local_ptr;
    if (parse_ast(parser, &root_node_ptr) != NECRO_SUCCESS)
    {
        necro_print_error(&parser->error, input_string, "Parsing");
        return;
    }
    print_ast(ast, &lexer->intern, root_node_ptr);
    if (compilation_phase == NECRO_PHASE_PARSE)
        return;

    //=====================================================
    // Reifying
    //=====================================================
    necro_announce_phase("Reifying");
    NecroAST_Reified ast_r = necro_reify_ast(ast, root_node_ptr, &lexer->intern);
    necro_print_reified_ast(&ast_r, &lexer->intern);
    *destruct_flags |= BIT(NECRO_PHASE_REIFY);
    if (compilation_phase == NECRO_PHASE_REIFY)
        return;

    //=====================================================
    // Build Scopes
    //=====================================================
    necro_announce_phase("Building Scopes");
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
    if (necro_build_scopes(&scoped_symtable, &ast_r) != NECRO_SUCCESS)
    {
        necro_print_error(&scoped_symtable.error, input_string, "Building Scopes");
        return;
    }
    necro_symtable_print(&symtable);
    necro_scoped_symtable_print(&scoped_symtable);
    if (compilation_phase == NECRO_PHASE_BUILD_SCOPES)
        return;

    //=====================================================
    // Renaming
    //=====================================================
    necro_announce_phase("Renaming");
    NecroRenamer renamer = necro_create_renamer(&scoped_symtable, &lexer->intern);
    *destruct_flags |= BIT(NECRO_PHASE_RENAME);
    if (necro_prim_rename(&prim_types, &renamer) != NECRO_SUCCESS)
    {
        necro_print_reified_ast(&ast_r, &lexer->intern);
        necro_print_error(&renamer.error, input_string, "Renaming (Prim Pass)");
        return;
    }
    if (necro_rename_declare_pass(&renamer, &ast_r.arena, ast_r.root) != NECRO_SUCCESS)
    {
        necro_print_reified_ast(&ast_r, &lexer->intern);
        necro_print_error(&renamer.error, input_string, "Renaming (Declare Pass)");
        return;
    }
    if (necro_rename_var_pass(&renamer, &ast_r.arena, ast_r.root) != NECRO_SUCCESS)
    {
        necro_print_reified_ast(&ast_r, &lexer->intern);
        necro_print_error(&renamer.error, input_string, "Renaming (Var Pass)");
        return;
    }
    necro_print_reified_ast(&ast_r, &lexer->intern);
    puts("");
    if (compilation_phase == NECRO_PHASE_RENAME)
        return;

    //=====================================================
    // Dependency Analyzing
    //=====================================================
    necro_announce_phase("Dependency Analysis");
    NecroDependencyAnalyzer d_analyzer = necro_create_dependency_analyzer(&symtable, &lexer->intern);
    // *type_class_env = necro_create_type_class_env(&scoped_symtable, &renamer);
    *infer = necro_create_infer(&lexer->intern, &symtable, &scoped_symtable, &renamer, &prim_types);
    *destruct_flags |= BIT(NECRO_PHASE_DEPENDENCY_ANALYSIS);
    if (necro_prim_infer(&prim_types, &d_analyzer, infer, compilation_phase) != NECRO_SUCCESS)
    {
        necro_print_error(&infer->error, input_string, "Prim Type");
        return;
    }
    if (necro_dependency_analyze_ast(&d_analyzer, &ast_r.arena, ast_r.root))
    {
        necro_print_error(&renamer.error, input_string, "Dependency Analysis");
        return;
    }
    if (compilation_phase == NECRO_PHASE_DEPENDENCY_ANALYSIS)
    {
        necro_print_reified_ast(&ast_r, &lexer->intern);
        return;
    }
    puts("");

    //=====================================================
    // Infer
    //=====================================================
    necro_announce_phase("Typing");
    necro_infer(infer, ast_r.root);
    necro_symtable_print(&symtable);
    necro_print_type_classes(infer);
    necro_print_env_with_symtable(&symtable, infer);
    *destruct_flags |= BIT(NECRO_PHASE_INFER);
    if (infer->error.return_code != NECRO_SUCCESS)
    {
        necro_print_error(&infer->error, input_string, "Type");
        return;
    }

    // TODO: Redo this once TypeClass refactor is done
    necro_type_class_translate(infer, ast_r.root);
    if (infer->error.return_code != NECRO_SUCCESS)
    {
        necro_print_reified_ast(&ast_r, &lexer->intern);
        necro_print_error(&infer->error, input_string, "Type");
        return;
    }

    necro_print_reified_ast(&ast_r, &lexer->intern);
    if (compilation_phase == NECRO_PHASE_INFER)
        return;

    //=====================================================
    // Transform to Core
    //=====================================================
    necro_announce_phase("Transforming to Core!");

    NecroCoreAST ast_core;
    ast_core.root = NULL;
    necro_construct_core_transform(core_transform, &ast_core, &ast_r, &lexer->intern, &prim_types);
    *destruct_flags |= BIT(NECRO_PHASE_TRANSFORM_TO_CORE);
    necro_transform_to_core(core_transform);
    if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
    {
        printf("Failed to transform to core.");
        //necro_print_error(&parser.error, input_string, "Transforming to Core");
        return;
    }

    necro_print_core(&ast_core, &lexer->intern);

    if (compilation_phase == NECRO_PHASE_TRANSFORM_TO_CORE)
        return;
}

bool validate_destruct_phase(NECRO_PHASE requested_phase, uint32_t destruct_flags)
{
    return (destruct_flags & BIT(requested_phase)) != 0;
}

void necro_compile(const char* input_string, NECRO_PHASE compilation_phase)
{
    uint32_t destruct_flags = 0;
    NecroInfer infer;
    // NecroTypeClassEnv type_class_env;
    NecroParser parser;
    NecroTransformToCore core_transform;
    NecroLexer lexer;
    NecroAST ast;

    necro_compile_impl(
        input_string,
        compilation_phase,
        &infer,
        // &type_class_env,
        &parser,
        &core_transform,
        &lexer,
        &ast,
        &destruct_flags);

    //=====================================================
    // Cleaning up
    //=====================================================

    necro_announce_phase("Cleaning Up");

    if (validate_destruct_phase(NECRO_PHASE_DEPENDENCY_ANALYSIS, destruct_flags))
    {
        necro_destroy_infer(&infer);
        // necro_destroy_type_class_env(&type_class_env);
    }

    if (validate_destruct_phase(NECRO_PHASE_PARSE, destruct_flags))
    {
        destruct_parser(&parser);
    }

    if (validate_destruct_phase(NECRO_PHASE_TRANSFORM_TO_CORE, destruct_flags))
    {
        necro_destruct_core_transform(&core_transform);
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
    case NECRO_TEST_VM:                necro_test_vm();                break;
    case NECRO_TEST_DVM:               necro_test_dvm();               break;
    case NECRO_TEST_SYMTABLE:          necro_symtable_test();          break;
    case NECRO_TEST_SLAB:              necro_test_slab();              break;
    case NECRO_TEST_TREADMILL:         necro_test_treadmill();         break;
    case NECRO_TEST_LEXER:             necro_test_lexer();             break;
    case NECRO_TEST_INTERN:            necro_test_intern();            break;
    case NECRO_TEST_VAULT:             necro_vault_test();             break;
    case NECRO_TEST_ARCHIVE:           necro_archive_test();           break;
    case NECRO_TEST_REGION:            necro_region_test();            break;
    case NECRO_TEST_INFER:             necro_test_infer();             break;
    case NECRO_TEST_TYPE:              necro_test_type();              break;
    case NECRO_TEST_ARENA_CHAIN_TABLE: necro_arena_chain_table_test(); break;
    case NECRO_TEST_ALL:
        necro_test_dvm();
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
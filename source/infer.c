/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include "infer.h"

// NecroInferResult necro_infer_ast_error(NecroInfer* infer, NecroType* type, NecroAST_Node_Reified* ast, const char* error_message, ...);

// static const NecroInferResult null_infer_result = { NULL, NULL };
// /*
// typedef enum
// {
//     NECRO_AST_UNDEFINED,
//     NECRO_AST_CONSTANT,
//     NECRO_AST_UN_OP,
//     NECRO_AST_BIN_OP,
//     NECRO_AST_IF_THEN_ELSE,
//     NECRO_AST_TOP_DECL,
//     NECRO_AST_DECL,
//     NECRO_AST_SIMPLE_ASSIGNMENT,
//     NECRO_AST_APATS_ASSIGNMENT,
//     NECRO_AST_RIGHT_HAND_SIDE,
//     NECRO_AST_LET_EXPRESSION,
//     NECRO_AST_FUNCTION_EXPRESSION,
//     NECRO_AST_VARIABLE,
//     NECRO_AST_APATS,
//     NECRO_AST_WILDCARD,
//     NECRO_AST_LAMBDA,
//     NECRO_AST_DO,
//     NECRO_AST_LIST_NODE,
//     NECRO_AST_EXPRESSION_LIST,
//     NECRO_AST_TUPLE,
//     NECRO_BIND_ASSIGNMENT,
//     NECRO_AST_ARITHMETIC_SEQUENCE,
//     NECRO_AST_CASE,
//     NECRO_AST_CASE_ALTERNATIVE,
//     NECRO_AST_CONID,
//     NECRO_AST_TYPE_APP,
//     NECRO_AST_BIN_OP_SYM,
//     NECRO_AST_CONSTRUCTOR,
//     NECRO_AST_SIMPLE_TYPE,
//     NECRO_AST_DATA_DECLARATION,
//     NECRO_AST_TYPE_CLASS_CONTEXT,
//     NECRO_AST_TYPE_CLASS_DECLARATION,
//     NECRO_AST_TYPE_CLASS_INSTANCE,
//     NECRO_AST_TYPE_SIGNATURE,
//     NECRO_AST_FUNCTION_TYPE,
//     // NECRO_AST_MODULE,
// } NecroAST_NodeType;
// */

// //=====================================================
// // Variable
// //=====================================================
// NecroType* necro_infer_var_go(NecroInfer* infer, NecroTypeScheme* scheme)
// {
//     assert(infer  != NULL);
//     assert(scheme != NULL);
//     if (scheme->type == NECRO_TYPE_SCHEME_TERM)
//     {
//         return &scheme->term;
//     }
//     else
//     {
//         NecroType* type_var = necro_create_type_var(infer, necro_new_name(infer));
//         NecroSub*  sub      = necro_create_sub(infer, type_var, scheme->for_all.var.id, NULL);
//         return necro_infer_var_go(infer, necro_sub_type_scheme(infer, sub, scheme));
//     }
// }

// inline NecroInferResult necro_infer_var(NecroInfer* infer, NecroGamma* gamma, NecroAST_Node_Reified* ast)
// {
//     assert(infer != NULL);
//     assert(ast != NULL);
//     if (infer->error.return_code != NECRO_SUCCESS)
//         return null_infer_result;
//     NecroTypeScheme* scheme = necro_gamma_find(infer, gamma, (NecroVar) { ast->variable.id });
//     if (scheme == NULL)
//         return necro_infer_ast_error(infer, NULL, ast, "Type not found for variable: %s", necro_intern_get_string(infer->intern, ast->variable.symbol));
//     NecroTypeScheme* result = necro_principal_type_scheme(infer, gamma, necro_infer_var_go(infer, scheme));
//     return (NecroInferResult) { NULL, result };
// }

// //=====================================================
// // Let
// //=====================================================
// // TODO: Recursive Let bindings
// inline NecroInferResult necro_infer_let(NecroInfer* infer, NecroGamma* gamma, NecroAST_Node_Reified* ast)
// {
//     return null_infer_result;
// }

// //=====================================================
// // Infer Go
// //=====================================================
// NecroInferResult necro_infer_go(NecroInfer* infer, NecroGamma* gamma, NecroAST_Node_Reified* ast)
// {
//     assert(infer != NULL);
//     if (infer->error.return_code != NECRO_SUCCESS)
//         return null_infer_result;
//     if (ast == NULL)
//         return null_infer_result;
//     switch (ast->type)
//     {
//     case NECRO_AST_VARIABLE:       return necro_infer_var(infer, gamma, ast);
//     case NECRO_AST_LET_EXPRESSION: return necro_infer_let(infer, gamma, ast);
//     default:                       return necro_infer_ast_error(infer, NULL, ast, "AST type %d has not been implemented for type inference", ast->type);
//     }
//     return null_infer_result;
// }

// NecroTypeScheme* necro_infer(NecroInfer* infer, NecroGamma* gamma, NecroAST_Node_Reified* ast)
// {
//     necro_check_gamma_sanity(infer, gamma);
//     NecroInferResult result = necro_infer_go(infer, gamma, ast);
//     necro_check_type_scheme_sanity(infer, result.type);
//     if (infer->error.return_code != NECRO_SUCCESS)
//     {
//         printf("Type error:\n    %s", infer->error.error_message);
//         printf("\n");
//         return NULL;
//     }
//     return result.type;
// }

// NecroInferResult necro_infer_ast_error(NecroInfer* infer, NecroType* type, NecroAST_Node_Reified* ast, const char* error_message, ...)
// {
//     if (infer->error.return_code != NECRO_SUCCESS)
//         return null_infer_result;
//     va_list args;
// 	va_start(args, error_message);

//     size_t count1 = necro_verror(&infer->error, ast->source_loc, error_message, args);
//     if (type != NULL)
//     {
//         size_t count2 = snprintf(infer->error.error_message + count1, NECRO_MAX_ERROR_MESSAGE_LENGTH, "\n    Found in type:\n        ");
//         necro_snprintf_type_sig(type, infer->intern, infer->error.error_message + count1 + count2, NECRO_MAX_ERROR_MESSAGE_LENGTH);
//     }

//     va_end(args);

//     return null_infer_result;
// }

// //=====================================================
// // Testing
// //=====================================================
// void necro_test_infer()
// {
//     necro_announce_phase("NecroInfer");

//     NecroIntern      intern = necro_create_intern();
//     NecroInfer       infer  = necro_create_infer(&intern);

//     // Test Var
//     {
//         NecroVar               avar     = { 1 };
//         NecroType*             icon     = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL, 0);
//         NecroTypeScheme*       term     = necro_create_term(&infer, icon);
//         NecroGamma*            gamma    = necro_create_gamma(&infer, avar, term, NULL);

//         NecroAST_Node_Reified* var_node = necro_paged_arena_alloc(&infer.arena, sizeof(NecroAST_Node_Reified));
//         var_node->type                  = NECRO_AST_VARIABLE;
//         var_node->variable.symbol       = necro_intern_string(&intern, "a");
//         var_node->variable.id.id        = 1;

//         NecroTypeScheme*       type     = necro_infer(&infer, gamma, var_node);
//         necro_print_type_scheme_test_result("var1Expect", necro_create_term(&infer, icon), "var1Result", type, &intern);
//     }

// }

void necro_test_infer()
{
}

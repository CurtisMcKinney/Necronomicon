/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "fcore.h"

// /*
//     * https://www.microsoft.com/en-us/research/wp-content/uploads/1992/01/student.pdf

//     * Closure conversion
//     * Constant hoisting / folding

//     notes:
//         * Lifting inner functions out is always ok.
//         * Lifting inner values out is only ok if it is constant. Otherwise moving it would change the semantics of the code (which is, obivously, a bad thing).
//         * Due to the semantics of necrolang we should lift as many constants out as possible? (use a constant map to share equivalant constants)
// */

// typedef struct
// {
//     NecroSnapshotArena snapshot_arena;
//     NecroFCore         fcore;
//     NecroIntern*       intern;
//     NecroSymTable*     symtable;
//     NecroPrimTypes*    prim_types;
//     size_t             gen_vars;
// } NecroClosureConversion;

// NecroClosureConversion necro_create_closure_conversion(NecroIntern* intern, NecroSymTable* symtable, NecroPrimTypes* prim_types)
// {
//     return (NecroClosureConversion)
//     {
//         .snapshot_arena = necro_create_snapshot_arena(),
//         .fcore          = (NecroFCore) { .arena = necro_create_paged_arena(), .ast = NULL },
//         .intern         = intern,
//         .symtable       = symtable,
//         .prim_types     = prim_types,
//         .gen_vars       = 0,
//     };
// }

// void necro_destroy_closure_conversion(NecroClosureConversion* closure_conversion)
// {
// }

// ///////////////////////////////////////////////////////
// // Utility
// ///////////////////////////////////////////////////////
// NecroVar necro_gen_var(NecroClosureConversion* closure_conversion, NecroType* type)
// {
//     NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&closure_conversion->snapshot_arena);
//     char buf[10];
//     itoa(closure_conversion->gen_vars++, buf, 10);
//     const char* var_name = necro_concat_strings(&closure_conversion->snapshot_arena, 2, (const char*[]) { "_genVar#", buf });
//     NecroSymbol var_sym  = necro_intern_string(closure_conversion->intern, var_name);
//     NecroID     var_id   = necro_symtable_manual_new_symbol(closure_conversion->symtable, var_sym);
//     necro_symtable_get(closure_conversion->symtable, var_id)->type = type;
//     necro_rewind_arena(&closure_conversion->snapshot_arena, snapshot);
//     return (NecroVar) { .id = var_id, .symbol = var_sym };
// }

// ///////////////////////////////////////////////////////
// // AST construction
// ///////////////////////////////////////////////////////
// void necro_init_fcore_meta_data(NecroFCoreAST* ast, NECRO_STATE_TYPE state_type, NecroType* necro_type)
// {
//     ast->state_type       = state_type;
//     ast->necro_type       = necro_type;
//     ast->persistent_slot  = 0;
//     ast->is_recursive     = false;
// }

// NecroFCoreAST* necro_create_fcore_ast_lit(NecroPagedArena* arena, NecroSymTable* symtable, NecroPrimTypes* prim_types, NecroAST_Constant_Reified lit)
// {
//     NecroFCoreAST* ast   = necro_paged_arena_alloc(arena, sizeof(NecroFCoreAST));
//     ast->type            = NECRO_FCORE_LIT;
//     ast->lit.type        = lit.type;
//     NecroType* type = NULL;
//     switch (lit.type)
//     {
//     case NECRO_AST_CONSTANT_INTEGER:
//         type = necro_symtable_get(symtable, prim_types->int_type.id)->type;
//         ast->lit.int_literal = lit.int_literal;
//         break;
//     case NECRO_AST_CONSTANT_INTEGER_PATTERN:
//         type = necro_symtable_get(symtable, prim_types->int_type.id)->type;
//         ast->lit.int_literal = lit.int_literal;
//         break;
//     case NECRO_AST_CONSTANT_FLOAT:
//         type = necro_symtable_get(symtable, prim_types->float_type.id)->type;
//         ast->lit.double_literal = lit.double_literal;
//         break;
//     case NECRO_AST_CONSTANT_FLOAT_PATTERN:
//         type = necro_symtable_get(symtable, prim_types->float_type.id)->type;
//         ast->lit.double_literal = lit.double_literal;
//         break;
//     case NECRO_AST_CONSTANT_CHAR:
//         type = necro_symtable_get(symtable, prim_types->char_type.id)->type;
//         ast->lit.char_literal = lit.char_literal;
//         break;
//     case NECRO_AST_CONSTANT_CHAR_PATTERN:
//         type = necro_symtable_get(symtable, prim_types->char_type.id)->type;
//         ast->lit.char_literal = lit.char_literal;
//         break;
//     }
//     necro_init_fcore_meta_data(ast, true, type);
//     return ast;
// }

// NecroFCoreAST* necro_create_fcore_ast_var(NecroPagedArena* arena, NecroSymTable* symtable, NecroID id, NecroSymbol symbol)
// {
//     NecroFCoreAST* ast       = necro_paged_arena_alloc(arena, sizeof(NecroFCoreAST));
//     ast->type                = NECRO_FCORE_VAR;
//     ast->var.id              = id;
//     ast->var.symbol          = symbol;
//     necro_init_fcore_meta_data(ast, false, necro_symtable_get(symtable, id)->type);
//     return ast;
// }

// NecroFCoreAST* necro_create_fcore_ast_data_con(NecroPagedArena* arena, NecroSymTable* symtable, NecroFCoreAST* args, size_t num_args, NecroVar conid, NecroFCoreDataCon* next)
// {
//     NecroFCoreAST* ast = necro_paged_arena_alloc(arena, sizeof(NecroFCoreAST));
//     ast->type              = NECRO_CORE_EXPR_DATA_CON;
//     ast->data_con.args     = args;
//     ast->data_con.num_args = num_args;
//     ast->data_con.condid   = conid;
//     ast->data_con.next     = next;
//     necro_init_fcore_meta_data(ast, false, necro_symtable_get(symtable, conid.id)->type);
//     return ast;
// }

// NecroFCoreAST* necro_create_fcore_ast_data_decl(NecroPagedArena* arena, NecroSymTable* symtable, NecroVar conid, NecroFCoreDataCon* cons, size_t num_cons)
// {
//     NecroFCoreAST* ast      = necro_paged_arena_alloc(arena, sizeof(NecroFCoreAST));
//     ast->type               = NECRO_FCORE_DATA_DECL;
//     ast->data_decl.data_id  = conid;
//     ast->data_decl.cons     = cons;
//     ast->data_decl.num_cons = num_cons;
//     necro_init_fcore_meta_data(ast, false, necro_symtable_get(symtable, conid.id)->type);
//     return ast;
// }

// NecroFCoreAST* necro_create_fcore_ast_case(NecroPagedArena* arena, NecroFCoreCaseAlt* alts, size_t num_alts, NecroFCoreAST* expr, NecroType* type)
// {
//     NecroFCoreAST* ast      = necro_paged_arena_alloc(arena, sizeof(NecroFCoreAST));
//     ast->type               = NECRO_FCORE_CASE;
//     ast->case_expr.alts     = alts;
//     ast->case_expr.num_alts = num_alts;
//     ast->case_expr.expr     = expr;
//     necro_init_fcore_meta_data(ast, false, type);
//     return ast;
// }

// NecroFCoreAST* necro_create_fcore_ast_app(NecroPagedArena* arena, NecroFCoreAST* fun, NecroFCoreAST* args, size_t num_args, NecroType* type)
// {
//     NecroFCoreAST* ast       = necro_paged_arena_alloc(arena, sizeof(NecroFCoreAST));
//     ast->type                = NECRO_FCORE_APP;
//     ast->app.fun             = fun;
//     ast->app.args            = args;
//     ast->app.num_args        = num_args;
//     necro_init_fcore_meta_data(ast, false, type);
//     return ast;
// }

// NecroFCoreBind* necro_create_fcore_ast_bind(NecroPagedArena* arena, NecroVar var, NecroFCoreAST* expr)
// {
//     NecroFCoreBind* bind = necro_paged_arena_alloc(arena, sizeof(NecroFCoreBind));
//     bind->var            = var;
//     bind->expr           = expr;
//     return bind;
// }

// NecroFCoreBindRec* necro_create_fcore_ast_bind_rec(NecroPagedArena* arena, NecroFCoreBind* binds, size_t num_binds)
// {
//     NecroFCoreBindRec* bind = necro_paged_arena_alloc(arena, sizeof(NecroFCoreBindRec));
//     bind->binds             = binds;
//     bind->num_binds         = num_binds;
//     return bind;
// }

// NecroFCoreAST* necro_create_fcore_ast_let(NecroPagedArena* arena, NecroSymTable* symtable, NecroFCoreBind* bind, NecroFCoreAST* expr)
// {
//     NecroFCoreAST* ast = necro_paged_arena_alloc(arena, sizeof(NecroFCoreAST));
//     ast->type          = NECRO_FCORE_LET;
//     ast->let.bind      = bind;
//     ast->let.body      = expr;
//     ast->let.bind_type = NECRO_FCORE_LET_BIND;
//     necro_init_fcore_meta_data(ast, false, necro_symtable_get(symtable, bind->var.id)->type);
//     return ast;
// }

// NecroFCoreAST* necro_create_fcore_ast_let_rec(NecroPagedArena* arena, NecroSymTable* symtable, NecroFCoreBindRec* bind_rec, NecroFCoreAST* expr)
// {
//     NecroFCoreAST* ast = necro_paged_arena_alloc(arena, sizeof(NecroFCoreAST));
//     ast->type          = NECRO_FCORE_LET;
//     ast->let.bind_rec  = bind_rec;
//     ast->let.body      = expr;
//     ast->let.bind_type = NECRO_FCORE_LET_BIND_REC;
//     necro_init_fcore_meta_data(ast, false, necro_symtable_get(symtable, bind_rec->binds[0].var.id)->type);
//     return ast;
// }

// NecroFCoreAST* necro_create_fcore_ast_fun(NecroPagedArena* arena, NecroSymTable* symtable, NecroVar name, NecroFCoreAST* parameters, size_t num_parameters, NecroFCoreAST* body, NecroVar* free_vars, size_t num_free_vars, NecroVar* free_global_vars, size_t num_free_global_vars)
// {
//     NecroFCoreAST* ast            = necro_paged_arena_alloc(arena, sizeof(NecroFCoreAST));
//     ast->type                     = NECRO_FCORE_FUN;
//     ast->fun.name                 = name;
//     ast->fun.parameters           = parameters;
//     ast->fun.num_parameters       = num_parameters;
//     ast->fun.body                 = body;
//     ast->fun.free_vars            = free_vars;
//     ast->fun.num_free_vars        = num_free_vars;
//     ast->fun.free_global_vars     = free_global_vars;
//     ast->fun.num_free_global_vars = num_free_global_vars;
//     necro_init_fcore_meta_data(ast, false, necro_symtable_get(symtable, name.id)->type);
//     return ast;
// }

// NecroFCoreAST* necro_create_fcore_ast_closure(NecroPagedArena* arena, NecroSymTable* symtable, NecroVar fun, NecroFCoreBind* env_binds, size_t num_env_binds)
// {
//     NecroFCoreAST* ast         = necro_paged_arena_alloc(arena, sizeof(NecroFCoreAST));
//     ast->type                  = NECRO_FCORE_LET;
//     ast->closure.fun           = fun;
//     ast->closure.env_binds     = env_binds;
//     ast->closure.num_env_binds = num_env_binds;
//     necro_init_fcore_meta_data(ast, false, necro_symtable_get(symtable, fun.id)->type);
//     return ast;
// }

// NecroFCoreAST* necro_create_fcore_ast_project(NecroPagedArena* arena, NecroSymTable* symtable, NecroFCoreAST* closure, NecroVar var)
// {
//     NecroFCoreAST* ast         = necro_paged_arena_alloc(arena, sizeof(NecroFCoreAST));
//     ast->type                  = NECRO_FCORE_LET;
//     ast->proj.closure          = closure;
//     ast->proj.var              = var;
//     necro_init_fcore_meta_data(ast, false, necro_symtable_get(symtable, var.id)->type);
//     return ast;
// }

// ///////////////////////////////////////////////////////
// // Forward declarations
// ///////////////////////////////////////////////////////
// NecroFCoreAST* necro_closure_conversion_go(NecroClosureConversion* ccon, NecroCoreAST_Expression* in_ast);

// ///////////////////////////////////////////////////////
// // Closure Conversion
// ///////////////////////////////////////////////////////
// NecroFCore necro_closure_conversion(NecroCoreAST* in_ast, NecroIntern* intern, NecroSymTable* symtable, NecroPrimTypes* prim_types)
// {
//     NecroClosureConversion closure_conversion = necro_create_closure_conversion(intern, symtable, prim_types);
//     return closure_conversion.fcore;
// }

// // NecroCoreAST necro_core_final_pass(NecroCoreAST* in_ast, NecroIntern* intern, NecroSymTable* symtable, NecroPrimTypes* prim_types)
// // {
// //     NecroCoreFinalPass final_pass = necro_create_final_pass(intern, symtable, prim_types);
// //     necro_core_final_pass_go(&final_pass, in_ast->root);
// //     return final_pass.core_ast;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_lit(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_LIT);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_lit(final_pass, in_ast->lit);
// //     return out_ast;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_var(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_VAR);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_var(final_pass, in_ast->var.id, in_ast->var.symbol);
// //     return out_ast;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_data_con(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_DATA_CON);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* args = necro_core_final_pass_go(final_pass, in_ast->data_con.arg_list);
// //     NecroCoreAST_DataCon*    next = NULL;
// //     if (in_ast->data_con.next != NULL)
// //     {
// //         NecroCoreAST_Expression dummy_ast;
// //         dummy_ast.expr_type = NECRO_CORE_EXPR_DATA_CON;
// //         dummy_ast.data_con  = *in_ast->data_con.next;
// //         next                = &necro_core_final_pass_data_con(final_pass, &dummy_ast)->data_con;
// //         if (necro_is_final_pass_error(final_pass)) return NULL;
// //     }
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_data_con(final_pass, args, in_ast->data_con.condid, next);
// //     return out_ast;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_data_decl(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_DATA_DECL);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression dummy_ast;
// //     dummy_ast.expr_type = NECRO_CORE_EXPR_DATA_CON;
// //     dummy_ast.data_con  = *in_ast->data_decl.con_list;
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_data_decl(final_pass, in_ast->data_decl.data_id, &necro_core_final_pass_go(final_pass, &dummy_ast)->data_con);
// //     return out_ast;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_list(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     if (in_ast == NULL) return NULL;
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_LIST);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* out_ast_head = necro_create_fcore_ast_list(final_pass, necro_core_final_pass_go(final_pass, in_ast->list.expr), NULL);
// //     NecroCoreAST_Expression* out_ast_curr = out_ast_head;
// //     in_ast                                = in_ast->list.next;
// //     while (in_ast != NULL)
// //     {
// //         out_ast_curr->list.next = necro_create_fcore_ast_list(final_pass, necro_core_final_pass_go(final_pass, in_ast->list.expr), NULL);
// //         if (necro_is_final_pass_error(final_pass)) return NULL;
// //         in_ast                  = in_ast->list.next;
// //     }
// //     return out_ast_head;
// // }

// // NecroCoreAST_CaseAlt* necro_core_final_pass_alt(NecroCoreFinalPass* final_pass, NecroCoreAST_CaseAlt* in_alt)
// // {
// //     assert(final_pass != NULL);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     if (in_alt == NULL) return NULL;
// //     NecroCoreAST_CaseAlt* out_alt = necro_paged_arena_alloc(&final_pass->core_ast.arena, sizeof(NecroCoreAST_CaseAlt));
// //     if (in_alt->altCon == NULL)
// //         out_alt->altCon = NULL;
// //     else
// //         out_alt->altCon = necro_core_final_pass_go(final_pass, in_alt->altCon);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     out_alt->expr = necro_core_final_pass_go(final_pass, in_alt->expr);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     out_alt->next = necro_core_final_pass_alt(final_pass, in_alt->next);
// //     return out_alt;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_case(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_CASE);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_CaseAlt*    alts    = necro_core_final_pass_alt(final_pass, in_ast->case_expr.alts);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* expr    = necro_core_final_pass_go(final_pass, in_ast->case_expr.expr);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_case(final_pass, alts, expr, in_ast->case_expr.type);
// //     return out_ast;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_lambda(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_LAM);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* arg     = necro_core_final_pass_go(final_pass, in_ast->lambda.arg);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* expr    = necro_core_final_pass_go(final_pass, in_ast->lambda.expr);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_lambda(final_pass, arg, expr);
// //     return out_ast;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_app(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_APP);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* exprA   = necro_core_final_pass_go(final_pass, in_ast->app.exprA);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* exprB   = necro_core_final_pass_go(final_pass, in_ast->app.exprB);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_app(final_pass, exprA, exprB);
// //     return out_ast;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_let(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_LET);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* bind    = necro_core_final_pass_go(final_pass, in_ast->let.bind);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* expr    = necro_core_final_pass_go(final_pass, in_ast->let.expr);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_let(final_pass, bind, expr);
// //     return out_ast;
// // }

// // // TODO: Finish!
// // NecroCoreAST_Expression* necro_core_final_pass_bind(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_BIND);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* expr    = necro_core_final_pass_go(final_pass, in_ast->bind.expr);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_bind(final_pass, in_ast->bind.var, in_ast->bind.expr, in_ast->bind.is_recursive);
// //     // //--------------------
// //     // // Make non-pointfree
// //     // //--------------------
// //     // NecroCoreAST_Expression* ast      = out_ast;
// //     // NecroType*               for_alls = ((NecroFCoreAST*)out_ast)->necro_type;
// //     // NecroType*               arrows   = NULL;
// //     // while (for_alls->type == NECRO_TYPE_FOR)
// //     // {
// //     //     NecroTypeClassContext* for_all_context = for_alls->for_all.context;
// //     //     while (for_all_context != NULL)
// //     //     {
// //     //         if (ast->expr_type == NECRO_CORE_EXPR_LAM)
// //     //         {
// //     //             ast = ast->lambda.expr;
// //     //         }
// //     //         else
// //     //         {
// //     //             // NecroVar gen_var = necro_gen_var(final_pass, for_alls)
// //     //         }
// //     //         for_all_context = for_all_context->next;
// //     //     }
// //     //     for_alls = for_alls->for_all.type;
// //     //     for_alls = necro_find(for_alls);
// //     // }
// //     // arrows = for_alls;
// //     // arrows = necro_find(arrows);
// //     // while (arrows->type == NECRO_TYPE_FUN)
// //     // {
// //     //     if (ast->expr_type == NECRO_CORE_EXPR_LAM)
// //     //     {
// //     //         ast = ast->lambda.expr;
// //     //     }
// //     //     else
// //     //     {
// //     //         NecroVar                 gen_var = necro_gen_var(final_pass, arrows->fun.type1);
// //     //         NecroCoreAST_Expression* var_ast = necro_create_fcore_ast_var(final_pass, gen_var.id, gen_var.symbol);
// //     //         out_ast                          = necro_create_fcore_ast_app(final_pass, out_ast, var_ast);
// //     //         NecroCoreAST_Expression* arg_ast = necro_create_fcore_ast_var(final_pass, gen_var.id, gen_var.symbol);
// //     //         // TODO: Add new var and lambda to lambdas ast!
// //     //     }
// //     //     arrows = arrows->fun.type2;
// //     //     arrows = necro_find(arrows);
// //     // }
// //     return out_ast;
// // }

// ///////////////////////////////////////////////////////
// // Go
// ///////////////////////////////////////////////////////
// NecroFCoreAST* necro_closure_conversion_go(NecroClosureConversion* ccon, NecroCoreAST_Expression* in_ast)
// {
//     assert(ccon != NULL);
//     assert(in_ast != NULL);
//     // if (necro_is_final_pass_error(final_pass)) return NULL;
//     // switch (in_ast->expr_type)
//     // {
//     // case NECRO_CORE_EXPR_VAR:       return necro_core_final_pass_var(final_pass, in_ast);
//     // case NECRO_CORE_EXPR_DATA_CON:  return necro_core_final_pass_data_con(final_pass, in_ast);
//     // case NECRO_CORE_EXPR_DATA_DECL: return necro_core_final_pass_data_decl(final_pass, in_ast);
//     // case NECRO_CORE_EXPR_LIT:       return necro_core_final_pass_lit(final_pass, in_ast);
//     // case NECRO_CORE_EXPR_CASE:      return necro_core_final_pass_case(final_pass, in_ast);
//     // case NECRO_CORE_EXPR_LAM:       return necro_core_final_pass_lambda(final_pass, in_ast);
//     // case NECRO_CORE_EXPR_APP:       return necro_core_final_pass_app(final_pass, in_ast);
//     // case NECRO_CORE_EXPR_BIND:      return necro_core_final_pass_bind(final_pass, in_ast);
//     // case NECRO_CORE_EXPR_LET:       return necro_core_final_pass_let(final_pass, in_ast);
//     // case NECRO_CORE_EXPR_LIST:      return necro_core_final_pass_list(final_pass, in_ast); // used for top decls not language lists
//     // case NECRO_CORE_EXPR_TYPE:      assert(false && "NECRO_CORE_EXPR_TYPE not implemented; why are you using it, hmmm?"); return NULL;
//     // default:                        assert(false && "Unimplemented AST type in necro_core_final_pass_go"); return NULL;
//     // }
//     return NULL;
// }
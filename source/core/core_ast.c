/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "core_ast.h"
#include "core_create.h"
#include "type.h"
#include "monomorphize.h"
#include "alias_analysis.h"

#define NECRO_CORE_DEBUG 0
#if NECRO_CORE_DEBUG
#define TRACE_CORE(...) printf(__VA_ARGS__)
#else
#define TRACE_CORE(...)
#endif

///////////////////////////////////////////////////////
// Core Ast Creation
///////////////////////////////////////////////////////
inline NecroCoreAst* necro_core_ast_alloc(NecroPagedArena* arena, NECRO_CORE_AST_TYPE ast_type)
{
    NecroCoreAst* ast = necro_paged_arena_alloc(arena, sizeof(NecroCoreAst));
    ast->ast_type     = ast_type;
    return ast;
}

NecroCoreAst* necro_core_ast_create_lit(NecroPagedArena* arena, NecroAstConstant constant)
{
    NecroCoreAst* ast = necro_core_ast_alloc(arena, NECRO_CORE_AST_LIT);
    ast->lit.type     = constant.type;
    switch (constant.type)
    {
    case NECRO_AST_CONSTANT_FLOAT_PATTERN:
    case NECRO_AST_CONSTANT_FLOAT:
        ast->lit.float_literal = constant.double_literal;
        break;
    case NECRO_AST_CONSTANT_INTEGER_PATTERN:
    case NECRO_AST_CONSTANT_INTEGER:
        ast->lit.int_literal = constant.int_literal;
        break;
    case NECRO_AST_CONSTANT_STRING:
        ast->lit.string_literal = constant.symbol;
        break;
    case NECRO_AST_CONSTANT_CHAR_PATTERN:
    case NECRO_AST_CONSTANT_CHAR:
        ast->lit.char_literal = constant.char_literal;
        break;
    case NECRO_AST_CONSTANT_TYPE_INT:
    case NECRO_AST_CONSTANT_TYPE_STRING:
        assert(false);
        break;
    }
    return ast;
}

NecroCoreAst* necro_core_ast_create_var(NecroPagedArena* arena, NecroCoreAstSymbol* ast_symbol)
{
    NecroCoreAst* ast   = necro_core_ast_alloc(arena, NECRO_CORE_AST_VAR);
    ast->var.ast_symbol = ast_symbol;
    return ast;
}

NecroCoreAst* necro_core_ast_create_bind(NecroPagedArena* arena, NecroCoreAstSymbol* ast_symbol, NecroCoreAst* expr)
{
    NecroCoreAst* ast    = necro_core_ast_alloc(arena, NECRO_CORE_AST_BIND);
    ast->bind.ast_symbol = ast_symbol;
    ast->bind.expr       = expr;
    return ast;
}

NecroCoreAst* necro_core_ast_create_let(NecroPagedArena* arena, NecroCoreAst* bind, NecroCoreAst* expr)
{
    NecroCoreAst* ast = necro_core_ast_alloc(arena, NECRO_CORE_AST_LET);
    ast->let.bind     = bind;
    ast->let.expr     = expr;
    return ast;
}

NecroCoreAst* necro_core_ast_create_lam(NecroPagedArena* arena, NecroCoreAst* arg, NecroCoreAst* expr)
{
    NecroCoreAst* ast = necro_core_ast_alloc(arena, NECRO_CORE_AST_LAM);
    ast->lambda.arg   = arg;
    ast->lambda.expr  = expr;
    return ast;
}

NecroCoreAst* necro_core_ast_create_app(NecroPagedArena* arena, NecroCoreAst* expr1, NecroCoreAst* expr2)
{
    NecroCoreAst* ast = necro_core_ast_alloc(arena, NECRO_CORE_AST_APP);
    ast->app.expr1    = expr1;
    ast->app.expr2    = expr2;
    return ast;
}

NecroCoreAst* necro_core_ast_create_data_con(NecroPagedArena* arena, NecroCoreAstSymbol* ast_symbol, NecroType* type)
{
    NecroCoreAst* ast        = necro_core_ast_alloc(arena, NECRO_CORE_AST_DATA_CON);
    ast->data_con.ast_symbol = ast_symbol;
    ast->data_con.type       = type;
    return ast;
}

NecroCoreAst* necro_core_ast_create_data_decl(NecroPagedArena* arena, NecroCoreAstSymbol* ast_symbol, NecroCoreAstList* con_list)
{
    NecroCoreAst* ast         = necro_core_ast_alloc(arena, NECRO_CORE_AST_DATA_DECL);
    ast->data_decl.ast_symbol = ast_symbol;
    ast->data_decl.con_list   = con_list;
    return ast;
}

NecroCoreAst* necro_core_ast_create_case(NecroPagedArena* arena, NecroCoreAst* expr, NecroCoreAstList* alts)
{
    NecroCoreAst* ast   = necro_core_ast_alloc(arena, NECRO_CORE_AST_CASE);
    ast->case_expr.expr = expr;
    ast->case_expr.alts = alts;
    return ast;
}

NecroCoreAst* necro_core_ast_create_case_alt(NecroPagedArena* arena, NecroCoreAst* pat, NecroCoreAst* expr)
{
    NecroCoreAst* ast  = necro_core_ast_alloc(arena, NECRO_CORE_AST_CASE_ALT);
    ast->case_alt.pat  = pat;
    ast->case_alt.expr = expr;
    return ast;
}

NecroCoreAst* necro_core_ast_create_for_loop(NecroPagedArena* arena, NecroCoreAst* range_init, NecroCoreAst* value_init, NecroCoreAst* index_arg, NecroCoreAst* value_arg, NecroCoreAst* expression)
{
    NecroCoreAst* ast  = necro_core_ast_alloc(arena, NECRO_CORE_AST_FOR);
    ast->for_loop.range_init = range_init;
    ast->for_loop.value_init = value_init;
    ast->for_loop.index_arg  = index_arg;
    ast->for_loop.value_arg  = value_arg;
    ast->for_loop.expression = expression;
    return ast;
}

// NecroCoreAst* necro_core_ast_deep_copy(NecroPagedArena* arena, NecroCoreAst* ast)
// {
// }

///////////////////////////////////////////////////////
// NecroCoreAstArena
///////////////////////////////////////////////////////
NecroCoreAstArena necro_core_ast_arena_empty()
{
    return (NecroCoreAstArena)
    {
        .arena = necro_paged_arena_empty(),
        .root  = NULL,
    };
}

NecroCoreAstArena necro_core_ast_arena_create()
{
    NecroPagedArena arena = necro_paged_arena_create();
    return (NecroCoreAstArena)
    {
        .arena = arena,
        .root  = NULL,
    };
}

void necro_core_ast_arena_destroy(NecroCoreAstArena* ast_arena)
{
    necro_paged_arena_destroy(&ast_arena->arena);
    *ast_arena = necro_core_ast_arena_empty();
}

///////////////////////////////////////////////////////
// Transform to Core
///////////////////////////////////////////////////////
typedef struct NecroCoreAstTransform
{
    NecroPagedArena*   arena;
    NecroIntern*       intern;
    NecroBase*         base;
    NecroAstArena*     ast_arena;
    NecroCoreAstArena* core_ast_arena;
} NecroCoreAstTransform;

NecroCoreAstTransform necro_core_ast_transform_empty()
{
    return (NecroCoreAstTransform)
    {
        .arena          = NULL,
        .intern         = NULL,
        .base           = NULL,
        .ast_arena      = NULL,
        .core_ast_arena = NULL,
    };
}

NecroCoreAstTransform necro_core_ast_transform_create(NecroIntern* intern, NecroBase* base, NecroAstArena* ast_arena, NecroCoreAstArena* core_ast_arena)
{
    necro_core_ast_arena_destroy(core_ast_arena);
    *core_ast_arena = necro_core_ast_arena_create();
    return (NecroCoreAstTransform)
    {
        .arena           = &core_ast_arena->arena,
        .intern          = intern,
        .base            = base,
        .ast_arena       = ast_arena,
        .core_ast_arena  = core_ast_arena,
    };
}

void necro_ast_transform_destroy(NecroCoreAstTransform* core_ast_transform)
{
    *core_ast_transform = necro_core_ast_transform_empty();
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_go(NecroCoreAstTransform* context, NecroAst* ast);
NecroResult(void) necro_ast_transform_to_core(NecroCompileInfo info, NecroIntern* intern, NecroBase* base, NecroAstArena* ast_arena, NecroCoreAstArena* core_ast_arena)
{
    UNUSED(info);
    NecroCoreAstTransform core_ast_transform = necro_core_ast_transform_create(intern, base, ast_arena, core_ast_arena);
    core_ast_transform.core_ast_arena->root  = necro_try_map(NecroCoreAst, void, necro_ast_transform_to_core_go(&core_ast_transform, ast_arena->root));
    return ok_void();
}

// static inline void print_tabs(uint32_t num_tabs)
// {
//     for (uint32_t i = 0; i < num_tabs; ++i)
//     {
//         printf(STRING_TAB);
//     }
// }
//
// void necro_print_core_node(NecroCoreAST_Expression* ast_node, NecroIntern* intern, uint32_t depth);
//
// void necro_print_core_node(NecroCoreAST_Expression* ast_node, NecroIntern* intern, uint32_t depth)
// {
//     assert(ast_node);
//     assert(intern);
//     print_tabs(depth);
//
//     switch (ast_node->expr_type)
//     {
//     case NECRO_CORE_EXPR_VAR:
//         printf("(Var: %s, %d)\n", ast_node->var.symbol->str, ast_node->var.id.id);
//         break;
//
//     case NECRO_CORE_EXPR_BIND:
//         if (!ast_node->bind.is_recursive)
//             printf("(Bind: %s, %d)\n", ast_node->bind.var.symbol->str, ast_node->bind.var.id.id);
//         else
//             printf("(BindRec: %s, %d)\n", ast_node->bind.var.symbol->str, ast_node->bind.var.id.id);
//         necro_print_core_node(ast_node->bind.expr, intern, depth + 1);
//         break;
//
//     case NECRO_CORE_EXPR_LIT:
//         {
//             switch (ast_node->lit.type)
//             {
//             case NECRO_AST_CONSTANT_FLOAT:
//             case NECRO_AST_CONSTANT_FLOAT_PATTERN:
//                 printf("(%f)\n", ast_node->lit.double_literal);
//                 break;
//
//             case NECRO_AST_CONSTANT_INTEGER:
//             case NECRO_AST_CONSTANT_INTEGER_PATTERN:
//     #if WIN32
//                 printf("(%lli)\n", ast_node->lit.int_literal);
//     #else
//                 printf("(%li)\n", ast_node->constant.int_literal);
//     #endif
//                 break;
//             case NECRO_AST_CONSTANT_STRING:
//                 {
//                     const char* string = ast_node->lit.symbol->str;
//                     if (string)
//                         printf("(\"%s\")\n", string);
//                 }
//                 break;
//
//             case NECRO_AST_CONSTANT_CHAR:
//             case NECRO_AST_CONSTANT_CHAR_PATTERN:
//                 printf("(\'%c\')\n", ast_node->lit.char_literal);
//                 break;
//
//             // case NECRO_AST_CONSTANT_BOOL:
//             //     printf("(%s)\n", ast_node->lit.boolean_literal ? "True" : "False");
//             //     break;
//
//             default:
//                 assert(false && "[necro_print_core_node] Unhandled literal type!");
//                 break;
//             }
//         }
//         break;
//
//     case NECRO_CORE_EXPR_APP:
//         puts("(App)");
//         necro_print_core_node(ast_node->app.expr1, intern, depth + 1);
//         necro_print_core_node(ast_node->app.expr2, intern, depth + 1);
//         break;
//
//     case NECRO_CORE_EXPR_LAM:
//         puts("(\\ ->)");
//         necro_print_core_node(ast_node->lambda.arg, intern, depth + 1);
//         necro_print_core_node(ast_node->lambda.expr, intern, depth + 1);
//         break;
//
//     case NECRO_CORE_EXPR_LET:
//         {
//             puts("(Let)");
//             necro_print_core_node(ast_node->let.bind, intern, depth + 1);
//             necro_print_core_node(ast_node->let.expr, intern, depth + 1);
//         }
//         break;
//
//     case NECRO_CORE_EXPR_CASE:
//         {
//             puts("(Case)");
//             necro_print_core_node(ast_node->case_expr.expr, intern, depth + 1);
//             NecroCoreAST_CaseAlt* alt = ast_node->case_expr.alts;
//
//             printf("\r");
//             print_tabs(depth + 1);
//             puts("(Of)");
//
//             while (alt)
//             {
//                 print_tabs(depth + 2);
//                 printf("(AltCon)\n");
//                 if (alt->altCon)
//                 {
//                     necro_print_core_node(alt->altCon, intern, depth + 3);
//                 }
//                 else
//                 {
//                     print_tabs(depth + 3);
//                     printf("_\n");
//                 }
//
//                 necro_print_core_node(alt->expr, intern, depth + 3);
//                 alt = alt->next;
//             }
//         }
//         break;
//
//     case NECRO_CORE_EXPR_LIST:
//         {
//             puts("(CORE_EXPR_LIST)");
//             // NecroCoreAST_List* list_expr = &ast_node->list;
//             NecroCoreAST_Expression* list_expr = ast_node;
//             while (list_expr)
//             {
//                 if (list_expr->list.expr)
//                 {
//                     necro_print_core_node(list_expr->list.expr, intern, depth + 1);
//                 }
//                 else
//                 {
//                     print_tabs(depth + 1);
//                     printf("_\n");
//                 }
//                 list_expr = list_expr->list.next;
//             }
//         }
//         break;
//
//     case NECRO_CORE_EXPR_DATA_DECL:
//         {
//             printf("(Data %s)\n", ast_node->data_decl.data_id.symbol->str);
//             NecroCoreAST_DataCon* con = ast_node->data_decl.con_list;
//             while (con)
//             {
//                 NecroCoreAST_Expression con_expr;
//                 con_expr.expr_type = NECRO_CORE_EXPR_DATA_CON;
//                 con_expr.data_con = *con;
//                 necro_print_core_node(&con_expr, intern, depth + 1);
//                 con = con->next;
//             }
//         }
//         break;
//
//     case NECRO_CORE_EXPR_DATA_CON:
//         printf("(DataCon: %s, %d)\n", ast_node->data_con.condid.symbol->str, ast_node->data_con.condid.id.id);
//         if (ast_node->data_con.arg_list)
//         {
//             necro_print_core_node(ast_node->data_con.arg_list, intern, depth + 1);
//         }
//         break;
//     case NECRO_CORE_EXPR_TYPE:
//     {
//         // char buf[1024];
//         // char* buf_end = necro_type_sig_snprintf(ast_node->type.type, intern, buf, 1024);
//         // *buf_end = '\0';
//         // printf("(Type: %s)\n", buf);
//         printf("(Type: ");
//         necro_type_print(ast_node->type.type);
//         break;
//     }
//
//     default:
//         printf("necro_print_core_node printing expression type unimplemented!: %s\n", core_ast_names[ast_node->expr_type]);
//         assert(false && "necro_print_core_node printing expression type unimplemented!");
//         break;
//     }
// }
//
// void necro_print_core(NecroCoreAST* ast, NecroIntern* intern)
// {
//     assert(ast != NULL);
//     necro_print_core_node(ast->root, intern, 0);
// }
//
// NecroTransformToCore necro_empty_core_transform()
// {
//     return (NecroTransformToCore)
//     {
//         .necro_ast  = NULL,
//         .core_ast   = NULL,
//         .intern     = NULL,
//         .prim_types = NULL,
//         .symtable   = NULL,
//     };
// }
//
// void necro_construct_core_transform(
//     NecroTransformToCore* core_transform,
//     NecroCoreAST* core_ast,
//     NecroAstArena* necro_ast,
//     NecroIntern* intern,
//     NecroPrimTypes* prim_types,
//     NecroSymTable* symtable,
//     NecroScopedSymTable* scoped_symtable)
// {
//     core_transform->core_ast = core_ast;
//     core_transform->core_ast->arena = necro_paged_arena_create();
//     core_transform->necro_ast = necro_ast;
//     core_transform->intern = intern;
//     core_transform->prim_types = prim_types;
//     core_transform->transform_state = NECRO_CORE_TRANSFORMING;
//     core_transform->symtable = symtable;
//
//     NecroCoreConstructors* constructors = &core_transform->constructors;
//     constructors->twoTuple =    necro_scoped_symtable_get_top_level_symbol_var(scoped_symtable, "(,)");
//     constructors->threeTuple =  necro_scoped_symtable_get_top_level_symbol_var(scoped_symtable, "(,,)");
//     constructors->fourTuple =   necro_scoped_symtable_get_top_level_symbol_var(scoped_symtable, "(,,,)");
//     constructors->fiveTuple =   necro_scoped_symtable_get_top_level_symbol_var(scoped_symtable, "(,,,,)");
//     constructors->sixTuple =    necro_scoped_symtable_get_top_level_symbol_var(scoped_symtable, "(,,,,,)");
//     constructors->sevenTuple =  necro_scoped_symtable_get_top_level_symbol_var(scoped_symtable, "(,,,,,,)");
//     constructors->eightTuple =  necro_scoped_symtable_get_top_level_symbol_var(scoped_symtable, "(,,,,,,,)");
//     constructors->nineTuple =   necro_scoped_symtable_get_top_level_symbol_var(scoped_symtable, "(,,,,,,,,)");
//     constructors->tenTuple =    necro_scoped_symtable_get_top_level_symbol_var(scoped_symtable, "(,,,,,,,,,)");
// }
//
// NecroCoreAST_Expression* necro_transform_to_core_impl(NecroTransformToCore* core_transform, NecroAst* necro_ast_node);
//
// // TODO/Note from Curtis: Boolean literals in necrolang don't exist. They need to be excised from the compiler completely.
// NecroCoreAST_Expression* necro_transform_if_then_else(NecroTransformToCore* core_transform, NecroAst* necro_ast_node)
// {
//     assert(core_transform);
//     assert(necro_ast_node);
//     assert(necro_ast_node->type == NECRO_AST_IF_THEN_ELSE);
//     if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
//         return NULL;
//
//     NecroAstIfThenElse* ast_if_then_else = &necro_ast_node->if_then_else;
//     NecroCoreAST_Expression* core_expr = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
//     core_expr->expr_type = NECRO_CORE_EXPR_CASE;
//
//     NecroCoreAST_Case* core_case = &core_expr->case_expr;
//     core_case->expr = necro_transform_to_core_impl(core_transform, ast_if_then_else->if_expr);
//     core_case->expr->necro_type = ast_if_then_else->if_expr->necro_type;
//     core_case->type = necro_ast_node->necro_type;
//
//     NecroCoreAST_CaseAlt* true_alt = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_CaseAlt));
//     true_alt->expr = necro_transform_to_core_impl(core_transform, ast_if_then_else->then_expr);
//     true_alt->altCon = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
//     // true_alt->altCon->lit.boolean_literal = true;
//     // true_alt->altCon->lit.type = NECRO_AST_CONSTANT_BOOL;
//     true_alt->altCon->var        = necro_con_to_var(necro_prim_types_get_data_con_from_symbol(core_transform->prim_types, necro_intern_string(core_transform->intern, "True")));
//     true_alt->altCon->expr_type  = NECRO_CORE_EXPR_VAR;
//     true_alt->altCon->necro_type = necro_symtable_get(core_transform->symtable, core_transform->prim_types->bool_type.id)->type;
//     true_alt->next = NULL;
//
//     NecroCoreAST_CaseAlt* false_alt = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_CaseAlt));
//     false_alt->expr = necro_transform_to_core_impl(core_transform, ast_if_then_else->else_expr);
//     // false_alt->altCon = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
//     // false_alt->altCon->lit.boolean_literal = false;
//     // false_alt->altCon->lit.type = NECRO_AST_CONSTANT_BOOL;
//     false_alt->altCon = NULL;
//     false_alt->next = NULL;
//     true_alt->next = false_alt;
//
//     core_case->alts = true_alt;
//     return core_expr;
// }
//
// NecroCoreAST_Expression* necro_transform_apats_assignment(NecroTransformToCore* core_transform, NecroAst* necro_ast_node)
// {
//     assert(core_transform);
//     assert(necro_ast_node);
//     assert(necro_ast_node->type == NECRO_AST_APATS_ASSIGNMENT);
//     if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
//         return NULL;
//
//     NecroAstApatsAssignment* apats_assignment = &necro_ast_node->apats_assignment;
//     NecroCoreAST_Expression* core_expr = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
//     core_expr->expr_type = NECRO_CORE_EXPR_BIND;
//     NecroCoreAST_Bind* core_bind = &core_expr->bind;
//     // core_bind->var.symbol = apats_assignment->variable_name; // TODO: CONVERT / FINISH! Core system needs to be converted to NecroAstSymbol system!!!!!!
//     // core_bind->var.id = apats_assignment->id; // TODO: CONVERT / FINISH! Core system needs to be converted to NecroAstSymbol system!!!!!!
//     core_bind->is_recursive = false;
//     NecroSymbolInfo* info = necro_symtable_get(core_transform->symtable, core_bind->var.id);
//     info->core_ast = core_expr;
//
//     if (apats_assignment->apats)
//     {
//         NecroAst lambda_node;
//         lambda_node.type = NECRO_AST_LAMBDA;
//         lambda_node.lambda.apats = apats_assignment->apats;
//         lambda_node.lambda.expression = apats_assignment->rhs;
//         core_bind->expr = necro_transform_to_core_impl(core_transform, &lambda_node);
//     }
//     else
//     {
//         core_bind->expr = necro_transform_to_core_impl(core_transform, apats_assignment->rhs);
//     }
//
//     return core_expr;
// }
//
// NecroCoreAST_Expression* necro_transform_pat_assignment(NecroTransformToCore* core_transform, NecroAst* necro_ast_node)
// {
//     assert(false && "[necro_transform_pat_assignment] Not implemented yet, soon TM!");
//     assert(core_transform);
//     assert(necro_ast_node);
//     assert(necro_ast_node->type == NECRO_AST_PAT_ASSIGNMENT);
//     if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
//         return NULL;
//
//     // NecroAstApatsAssignment* apats_assignment = &necro_ast_node->apats_assignment; // TODO: CONVERT / FINISH! Core system needs to be converted to NecroAstSymbol system!!!!!!
//     NecroCoreAST_Expression* core_expr = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
//     core_expr->expr_type = NECRO_CORE_EXPR_BIND;
//     NecroCoreAST_Bind* core_bind = &core_expr->bind;
//     // core_bind->var.symbol = apats_assignment->variable_name; // TODO: CONVERT / FINISH! Core system needs to be converted to NecroAstSymbol system!!!!!!
//     // core_bind->var.id = apats_assignment->id; // TODO: CONVERT / FINISH! Core system needs to be converted to NecroAstSymbol system!!!!!!
//     core_bind->is_recursive = false;
//
//     //necro_ast_node->pat_assignment.pat->pattern_expression.expressions->
//
//     return NULL;
// }
//
// NecroCoreAST_Expression* necro_transform_right_hand_side(NecroTransformToCore* core_transform, NecroAst* necro_ast_node)
// {
//     assert(core_transform);
//     assert(necro_ast_node);
//     assert(necro_ast_node->type == NECRO_AST_RIGHT_HAND_SIDE);
//     if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
//         return NULL;
//
//     NecroAstRightHandSide* right_hand_side = &necro_ast_node->right_hand_side;
//     if (right_hand_side->declarations)
//     {
//         assert(right_hand_side->declarations->type == NECRO_AST_DECL);
//         NecroAst* group_list = right_hand_side->declarations;
//         assert(group_list->type == NECRO_AST_DECLARATION_GROUP_LIST);
//         NecroCoreAST_Expression* core_let_expr = NULL;
//         NecroCoreAST_Let* core_let = NULL;
//         NecroCoreAST_Expression* rhs_expression = necro_transform_to_core_impl(core_transform, right_hand_side->expression);
//         while (group_list)
//         {
//             NecroAst* group = group_list->declaration_group_list.declaration_group;
//             while (group != NULL)
//             {
//                 assert(group);
//                 assert(group->type == NECRO_AST_DECL);
//                 assert(group->declaration.declaration_impl->type == NECRO_AST_SIMPLE_ASSIGNMENT);
//                 // NOTE - Curtis: Why ^^^? Kicking the can on nested functions!?!?!?
//                 NecroAstSimpleAssignment* simple_assignment = &group->declaration.declaration_impl->simple_assignment;
//                 NecroCoreAST_Expression* let_expr = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
//                 let_expr->expr_type = NECRO_CORE_EXPR_LET;
//                 NecroCoreAST_Let* let = &let_expr->let;
//                 let->bind = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
//                 let->bind->expr_type = NECRO_CORE_EXPR_BIND;
//                 // let->bind->bind.var.id = simple_assignment->id; // TODO: CONVERT / FINISH! Core system needs to be converted to NecroAstSymbol system!!!!!!
//                 // let->bind->bind.var.symbol = simple_assignment->variable_name; // TODO: CONVERT CORE TO USE NEW NecroAstSymbol system!
//                 let->bind->bind.is_recursive = simple_assignment->is_recursive;
//                 necro_symtable_get(core_transform->symtable, let->bind->bind.var.id)->core_ast = let_expr;
//                 let->bind->bind.expr = necro_transform_to_core_impl(core_transform, simple_assignment->rhs);
//                 if (core_let)
//                 {
//                     core_let->expr = let_expr;
//                 }
//                 else
//                 {
//                     core_let_expr = let_expr;
//                 }
//
//                 core_let = let;
//                 core_let->expr = rhs_expression;
//                 group = group->declaration.next_declaration;
//             }
//             group_list = group_list->declaration_group_list.next;
//         }
//
//         return core_let_expr;
//     }
//     else
//     {
//         return necro_transform_to_core_impl(core_transform, right_hand_side->expression);
//     }
// }
//
// NecroCoreAST_Expression* necro_transform_let(NecroTransformToCore* core_transform, NecroAst* necro_ast_node)
// {
//     assert(core_transform);
//     assert(necro_ast_node);
//     assert(necro_ast_node->type == NECRO_AST_LET_EXPRESSION);
//     if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
//         return NULL;
//
//     NecroAstLetExpression* reified_ast_let = &necro_ast_node->let_expression;
//     if (reified_ast_let->declarations)
//     {
//         assert(reified_ast_let->declarations->type == NECRO_AST_DECL);
//         NecroAst* group_list = reified_ast_let->declarations;
//         assert(group_list->type == NECRO_AST_DECLARATION_GROUP_LIST);
//         NecroCoreAST_Expression* core_let_expr = NULL;
//         NecroCoreAST_Let* core_let = NULL;
//         NecroCoreAST_Expression* rhs_expression = necro_transform_to_core_impl(core_transform, reified_ast_let->expression);
//         while (group_list)
//         {
//             NecroAst* group = group_list->declaration_group_list.declaration_group;
//             while (group != NULL)
//             {
//                 assert(group);
//                 assert(group->type == NECRO_AST_DECL);
//                 assert(group->declaration.declaration_impl->type == NECRO_AST_SIMPLE_ASSIGNMENT);
//                 NecroAstSimpleAssignment* simple_assignment = &group->declaration.declaration_impl->simple_assignment;
//                 NecroCoreAST_Expression* let_expr = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
//                 let_expr->expr_type = NECRO_CORE_EXPR_LET;
//                 NecroCoreAST_Let* let = &let_expr->let;
//                 let->bind = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
//                 let->bind->expr_type = NECRO_CORE_EXPR_BIND;
//                 // let->bind->bind.var.id = simple_assignment->id; // TODO: CONVERT / FINISH! Core system needs to be converted to NecroAstSymbol system!!!!!!
//                 // let->bind->bind.var.symbol = simple_assignment->variable_name; // TODO: Convert core over to use new NecroAstSymbol system!!!!
//                 necro_symtable_get(core_transform->symtable, let->bind->bind.var.id)->core_ast = let_expr;
//                 let->bind->bind.expr = necro_transform_to_core_impl(core_transform, simple_assignment->rhs);
//                 let->bind->bind.is_recursive = simple_assignment->is_recursive;
//                 if (core_let)
//                 {
//                     core_let->expr = let_expr;
//                 }
//                 else
//                 {
//                     core_let_expr = let_expr;
//                 }
//
//                 core_let = let;
//                 core_let->expr = rhs_expression;
//                 group = group->declaration.next_declaration;
//             }
//             group_list = group_list->declaration_group_list.next;
//         }
//
//         return core_let_expr;
//     }
//     else
//     {
//         assert(false && "Let requires a binding!");
//         return NULL;
//     }
// }
//
// NecroCoreAST_Expression* necro_transform_top_decl(NecroTransformToCore* core_transform, NecroAst* necro_ast_node)
// {
//     assert(core_transform);
//     assert(necro_ast_node);
//     assert(necro_ast_node->type == NECRO_AST_TOP_DECL);
//     if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
//         return NULL;
//
//     // NOTE (Curtis, 2-13-19); New system has NECRO_AST_DECLARATION_GROUP_LIST subsume NECRO_AST_TOP_DECL after dependency analysis
//     assert(false);
//     return NULL;
//
//     // NecroCoreAST_Expression* top_expression = NULL;
//     // NecroCoreAST_Expression* next_node = NULL;
//     // NecroDeclarationGroupList* group_list = necro_ast_node->top_declaration.group_list;
//     // assert(group_list);
//     // while (group_list)
//     // {
//     //     NecroDeclarationGroup* group = group_list->declaration_group;
//     //     while (group != NULL)
//     //     {
//     //         assert(group);
//     //         NecroCoreAST_Expression* last_node = next_node;
//     //         NecroCoreAST_Expression* expression = necro_transform_to_core_impl(core_transform, (NecroAst*) group->declaration_ast);
//     //         next_node = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
//     //         next_node->expr_type = NECRO_CORE_EXPR_LIST;
//     //         next_node->list.expr = expression;
//     //         next_node->list.next = NULL;
//     //         if (top_expression == NULL)
//     //         {
//     //             top_expression = next_node;
//     //         }
//     //         else
//     //         {
//     //             last_node->list.next = next_node;
//     //         }
//     //         group = group->next;
//     //     }
//     //     group_list = group_list->next;
//     // }
//
//     // assert(top_expression);
//     // return top_expression;
// }
//
// NecroCoreAST_Expression* necro_transform_expression_list(NecroTransformToCore* core_transform, NecroAst* necro_ast_node)
// {
//     assert(core_transform);
//     assert(necro_ast_node);
//     assert(necro_ast_node->type == NECRO_AST_EXPRESSION_LIST);
//     if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
//         return NULL;
//
//     NecroCoreAST_Expression* core_expr = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
//     core_expr->expr_type = NECRO_CORE_EXPR_VAR;
//     core_expr->var.symbol = core_transform->prim_types->list_type.symbol;
//     core_expr->var.id = core_transform->prim_types->list_type.id;
//
//     while (necro_ast_node->expression_list.expressions)
//     {
//         NecroCoreAST_Expression* cons_expr = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
//         cons_expr->expr_type = NECRO_CORE_EXPR_VAR;
//         //cons_expr->var.symbol = core_transform->prim_types-; // FINISH THIS!
//         cons_expr->var.id = core_transform->prim_types->list_type.id;
//     }
//
//     return core_expr;
// }
//
// NecroCoreAST_Expression* necro_transform_case(NecroTransformToCore* core_transform, NecroAst* necro_ast_node)
// {
//     assert(core_transform);
//     assert(necro_ast_node);
//     assert(necro_ast_node->type == NECRO_AST_CASE);
//     if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
//         return NULL;
//
//     NecroAstCase* case_ast = &necro_ast_node->case_expression;
//     NecroCoreAST_Expression* core_expr = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
//     core_expr->expr_type = NECRO_CORE_EXPR_CASE;
//     NecroCoreAST_Case* core_case = &core_expr->case_expr;
//     core_case->expr = necro_transform_to_core_impl(core_transform, case_ast->expression);
//     core_case->expr->necro_type = case_ast->expression->necro_type;
//     core_case->alts = NULL;
//     core_case->type = necro_ast_node->necro_type;
//
//     NecroAst* list_node = case_ast->alternatives;
//     // NecroAST_Node_Reified* alt_list_node = case_ast->alternatives;
//     // NecroAST_ListNode_Reified* list_node = &alt_list_node->list;
//     NecroCoreAST_CaseAlt* case_alt = NULL;
//
//     while (list_node)
//     {
//         NecroAst* alt_node = list_node->list.item;
//         assert(list_node->list.item->type == NECRO_AST_CASE_ALTERNATIVE);
//         NecroAstCaseAlternative* alt = &alt_node->case_alternative;
//
//         NecroCoreAST_CaseAlt* last_case_alt = case_alt;
//         case_alt = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_CaseAlt));
//         case_alt->expr = necro_transform_to_core_impl(core_transform, alt->body);
//         case_alt->next = NULL;
//         case_alt->altCon = NULL;
//
//         switch (alt->pat->type)
//         {
//
//         case NECRO_AST_CONSTANT:
//             case_alt->altCon = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
//             case_alt->altCon->expr_type = NECRO_CORE_EXPR_LIT;
//             case_alt->altCon->lit = alt->pat->constant;
//             break;
//
//         case NECRO_AST_WILDCARD:
//             case_alt->altCon = NULL;
//             break;
//
//         case NECRO_AST_CONSTRUCTOR:
//             case_alt->altCon = necro_transform_data_constructor(core_transform, alt->pat);
//             assert(case_alt->altCon->expr_type == NECRO_CORE_EXPR_DATA_CON);
//             break;
//
//         case NECRO_AST_CONID:
//             {
//                 case_alt->altCon = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
//                 case_alt->altCon->expr_type = NECRO_CORE_EXPR_DATA_CON;
//
//                 NecroCoreAST_DataCon* core_datacon = &case_alt->altCon->data_con;
//                 // core_datacon->condid.id = alt->pat->conid.id; // TODO: CONVERT / FINISH! Core system needs to be converted to NecroAstSymbol system!!!!!!
//                 // core_datacon->condid.symbol = alt->pat->conid.symbol; // TODO: CONVERT / FINISH! Needs to be converted to NecroAstSymbol system!!!!!!
//                 core_datacon->next = NULL;
//                 core_datacon->arg_list = NULL;
//             }
//             break;
//
//         case NECRO_AST_TUPLE:
//             case_alt->altCon = necro_transform_tuple(core_transform, alt->pat);
//             break;
//
//         default:
//             printf("necro_transform_case pattern type not implemented!: %d\n", alt->pat->type);
//             assert(false && "necro_transform_case pattern type not implemented!");
//             return NULL;
//         }
//
//         if (last_case_alt)
//         {
//             last_case_alt->next = case_alt;
//         }
//         else
//         {
//             core_case->alts = case_alt;
//         }
//
//         list_node = list_node->list.next_item;
//     }
//
//     return core_expr;
// }

NecroResult(NecroCoreAst) necro_ast_transform_to_core_var(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_VARIABLE);
    NecroCoreAst* core_ast = necro_core_ast_create_var(context->arena, necro_core_ast_symbol_create_from_ast_symbol(context->arena, ast->variable.ast_symbol));
    core_ast->necro_type   = necro_type_deep_copy(context->arena, ast->necro_type);
    return ok(NecroCoreAst, core_ast);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_con(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_CONID);
    NecroCoreAst* core_ast = necro_core_ast_create_var(context->arena, necro_core_ast_symbol_create_from_ast_symbol(context->arena, ast->conid.ast_symbol));
    core_ast->necro_type   = necro_type_deep_copy(context->arena, ast->necro_type);
    return ok(NecroCoreAst, core_ast);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_lit(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_CONSTANT);
    // Technically this is a bit dubious since we're doing type punning...
    NecroAstConstant constant;
    constant.int_literal   = ast->constant.int_literal;
    constant.type          = ast->constant.type;
    NecroCoreAst* core_ast = necro_core_ast_create_lit(context->arena, constant);
    core_ast->necro_type   = necro_type_deep_copy(context->arena, ast->necro_type);
    return ok(NecroCoreAst, core_ast);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_app(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_FUNCTION_EXPRESSION);
    NecroCoreAst* expr1    = necro_try(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->fexpression.aexp));
    NecroCoreAst* expr2    = necro_try(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->fexpression.next_fexpression));
    NecroCoreAst* core_ast = necro_core_ast_create_app(context->arena,expr1, expr2);
    core_ast->necro_type   = necro_type_deep_copy(context->arena, ast->necro_type);
    // core_ast->app.persistent_slot = 0; // Curtis: Metadata for codegen (would really prefer a constructor...)
    return ok(NecroCoreAst, core_ast);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_data_decl(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_DATA_DECLARATION);
    NecroCoreAst* core_ast  = necro_core_ast_create_data_decl(context->arena, necro_core_ast_symbol_create_from_ast_symbol(context->arena, ast->data_declaration.simpletype->simple_type.type_con->conid.ast_symbol), NULL);
    core_ast->necro_type    = core_ast->data_decl.ast_symbol->type;
    NecroAst*     cons      = ast->data_declaration.constructor_list;
    while (cons != NULL)
    {
        NecroCoreAst*  con_ast       = necro_core_ast_create_data_con(context->arena, necro_core_ast_symbol_create_from_ast_symbol(context->arena, cons->list.item->constructor.conid->conid.ast_symbol), NULL);
        con_ast->data_con.type       = con_ast->data_con.ast_symbol->type;
        core_ast->data_decl.con_list = necro_append_core_ast_list(context->arena, con_ast, core_ast->data_decl.con_list);
        cons                         = cons->list.next_item;
    }
    return ok(NecroCoreAst, core_ast);
}

// TODO: Filter out type signatures, class declarations, polymorphic values, etc
bool necro_core_ast_should_filter(NecroAst* ast)
{
    UNUSED(ast);
    return false;
}

// Going to do this the simple way for now...Flattening everything into singular Bind instead of using BindRecs for recursive groups
NecroResult(NecroCoreAst) necro_ast_transform_to_core_declaration_group_list(NecroCoreAstTransform* context, NecroAst* ast, NecroCoreAst* last_expr)
{
    if (ast == NULL)
        return ok(NecroCoreAst, last_expr);
    assert(ast->type == NECRO_AST_DECLARATION_GROUP_LIST);
    NecroCoreAst* let_head = NULL;
    NecroCoreAst* let_curr = NULL;
    while (ast != NULL)
    {
        NecroAst* declaration_group = ast->declaration_group_list.declaration_group;
        while (declaration_group != NULL)
        {
            if (!necro_core_ast_should_filter(declaration_group->declaration.declaration_impl))
            {
                NecroCoreAst* binder = necro_try(NecroCoreAst, necro_ast_transform_to_core_go(context, declaration_group->declaration.declaration_impl));
                assert(binder->ast_type == NECRO_CORE_AST_DATA_DECL || binder->ast_type == NECRO_CORE_AST_BIND || binder->ast_type == NECRO_CORE_AST_BIND_REC);
                if (let_head == NULL)
                {
                    let_curr = necro_core_ast_create_let(context->arena, binder, NULL);
                    let_head = let_curr;
                }
                else
                {
                    let_curr->let.expr = necro_core_ast_create_let(context->arena, binder, NULL);
                    let_curr           = let_curr->let.expr;
                }
            }
            declaration_group = declaration_group->declaration.next_declaration;
        }
        ast = ast->declaration_group_list.next;
    }
    if (let_head == NULL)
        return ok(NecroCoreAst, NULL);
    let_curr->let.expr = last_expr;
    return ok(NecroCoreAst, let_head);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_let(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_LET_EXPRESSION);
    NecroCoreAst* expr_ast  = necro_try(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->let_expression.expression));
    NecroCoreAst* binds_ast = necro_try(NecroCoreAst, necro_ast_transform_to_core_declaration_group_list(context, ast->let_expression.declarations, expr_ast));
    return ok(NecroCoreAst, binds_ast);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_rhs(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_RIGHT_HAND_SIDE);
    NecroCoreAst* expr_ast  = necro_try(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->right_hand_side.expression));
    NecroCoreAst* binds_ast = necro_try(NecroCoreAst, necro_ast_transform_to_core_declaration_group_list(context, ast->right_hand_side.declarations, expr_ast));
    return ok(NecroCoreAst, binds_ast);
}

// TODO: Patterns in Lambda Apats
NecroResult(NecroCoreAst) necro_ast_transform_to_core_lambda_apats(NecroCoreAstTransform* context, NecroAst* ast, NecroCoreAst* lambda_expr)
{
    NecroCoreAst* lambda_head = NULL;
    NecroCoreAst* lambda_curr = NULL;
    NecroAst*     apats       = ast->lambda.apats;
    while (apats != NULL)
    {
        if (apats->apats.apat->type == NECRO_AST_VARIABLE)
        {
            NecroCoreAst* var  = necro_try(NecroCoreAst, necro_ast_transform_to_core_go(context, apats->apats.apat));
            if (lambda_head == NULL)
            {
                lambda_curr = necro_core_ast_create_lam(context->arena, var, NULL);
                lambda_head = lambda_curr;
            }
            else
            {
                lambda_curr->lambda.expr = necro_core_ast_create_lam(context->arena, var, NULL);
                lambda_curr = lambda_curr->lambda.expr;
            }
        }
        else
        {
            // Note: We should float all case statements to being after all the lambdas and right before the lambda_expr.
            assert(false && "TODO: lambda apat patterns aren't implemented yet.");
        }
        apats = apats->apats.next_apat;
    }
    lambda_curr->lambda.expr = lambda_expr;
    return ok(NecroCoreAst, lambda_head);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_lambda(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_LAMBDA);
    NecroCoreAst* expr_ast = necro_try(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->lambda.expression));
    return necro_ast_transform_to_core_lambda_apats(context, ast, expr_ast);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_apats_assignment(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_APATS_ASSIGNMENT);
    NecroCoreAst* rhs_ast    = necro_try(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->apats_assignment.rhs));
    NecroCoreAst* lambda_ast = necro_try(NecroCoreAst, necro_ast_transform_to_core_lambda_apats(context, ast, rhs_ast));
    NecroCoreAst* bind_ast   = necro_core_ast_create_bind(context->arena, necro_core_ast_symbol_create_from_ast_symbol(context->arena, ast->apats_assignment.ast_symbol), lambda_ast);
    return ok(NecroCoreAst, bind_ast);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_simple_assignment(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_SIMPLE_ASSIGNMENT);
    // TODO: How to handle initializers?
    NecroCoreAst* expr_ast = necro_try(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->simple_assignment.rhs));
    NecroCoreAst* bind_ast = necro_core_ast_create_bind(context->arena, necro_core_ast_symbol_create_from_ast_symbol(context->arena, ast->simple_assignment.ast_symbol), expr_ast);
    return ok(NecroCoreAst, bind_ast);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_bin_op(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_BIN_OP);
    // NOTE: 1 + 2 --> (+) 1 2 --> ((+) 1) 2 --> App (App (+) 1) 2
    NecroCoreAst* op_ast    = necro_core_ast_create_var(context->arena, necro_core_ast_symbol_create_from_ast_symbol(context->arena, ast->bin_op.ast_symbol));
    NecroCoreAst* left_ast  = necro_try(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->bin_op.lhs));
    NecroCoreAst* right_ast = necro_try(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->bin_op.lhs));
    NecroCoreAst* inner_app = necro_core_ast_create_app(context->arena, op_ast, left_ast);
    NecroCoreAst* outer_app = necro_core_ast_create_app(context->arena, inner_app, right_ast);
    return ok(NecroCoreAst, outer_app);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_tuple(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_TUPLE);
    // Count args
    NecroAst* args  = ast->tuple.expressions;
    size_t    count = 0;
    while (args != NULL)
    {
        count++;
        args = args->list.next_item;
    }
    // Create Con var
    NecroCoreAst* tuple_con = NULL;
    switch (count)
    {
    case 2:  tuple_con = necro_core_ast_create_var(context->arena, necro_core_ast_symbol_create_from_ast_symbol(context->arena, context->base->tuple2_con)); break;
    case 3:  tuple_con = necro_core_ast_create_var(context->arena, necro_core_ast_symbol_create_from_ast_symbol(context->arena, context->base->tuple3_con)); break;
    case 4:  tuple_con = necro_core_ast_create_var(context->arena, necro_core_ast_symbol_create_from_ast_symbol(context->arena, context->base->tuple4_con)); break;
    case 5:  tuple_con = necro_core_ast_create_var(context->arena, necro_core_ast_symbol_create_from_ast_symbol(context->arena, context->base->tuple5_con)); break;
    case 6:  tuple_con = necro_core_ast_create_var(context->arena, necro_core_ast_symbol_create_from_ast_symbol(context->arena, context->base->tuple6_con)); break;
    case 7:  tuple_con = necro_core_ast_create_var(context->arena, necro_core_ast_symbol_create_from_ast_symbol(context->arena, context->base->tuple7_con)); break;
    case 8:  tuple_con = necro_core_ast_create_var(context->arena, necro_core_ast_symbol_create_from_ast_symbol(context->arena, context->base->tuple8_con)); break;
    case 9:  tuple_con = necro_core_ast_create_var(context->arena, necro_core_ast_symbol_create_from_ast_symbol(context->arena, context->base->tuple9_con)); break;
    case 10: tuple_con = necro_core_ast_create_var(context->arena, necro_core_ast_symbol_create_from_ast_symbol(context->arena, context->base->tuple10_con)); break;
    default: assert(false && "Unsupported tuple arity"); break;
    }
    // Apply args
    NecroCoreAst* tuple_ast = tuple_con;
    args                    = ast->tuple.expressions;
    while (args != NULL)
    {
        NecroCoreAst* arg_ast = necro_try(NecroCoreAst, necro_ast_transform_to_core_go(context, args->list.item));
        tuple_ast             = necro_core_ast_create_app(context->arena, tuple_ast, arg_ast);
        args = args->list.next_item;
    }
    return ok(NecroCoreAst, tuple_ast);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_go(NecroCoreAstTransform* context, NecroAst* ast)
{
    UNUSED(context);
    if (ast == NULL)
        return ok(NecroCoreAst, NULL);
    switch (ast->type)
    {
    case NECRO_AST_DECLARATION_GROUP_LIST: return necro_ast_transform_to_core_declaration_group_list(context, ast, NULL); // NOTE: Seems a little sketchy...
    case NECRO_AST_VARIABLE:               return necro_ast_transform_to_core_var(context, ast);
    case NECRO_AST_CONID:                  return necro_ast_transform_to_core_con(context, ast);
    case NECRO_AST_CONSTANT:               return necro_ast_transform_to_core_lit(context, ast);
    case NECRO_AST_FUNCTION_EXPRESSION:    return necro_ast_transform_to_core_app(context, ast);
    case NECRO_AST_DATA_DECLARATION:       return necro_ast_transform_to_core_data_decl(context, ast);
    case NECRO_AST_SIMPLE_ASSIGNMENT:      return necro_ast_transform_to_core_simple_assignment(context, ast);
    case NECRO_AST_LET_EXPRESSION:         return necro_ast_transform_to_core_let(context, ast);
    case NECRO_AST_RIGHT_HAND_SIDE:        return necro_ast_transform_to_core_rhs(context, ast);
    case NECRO_AST_LAMBDA:                 return necro_ast_transform_to_core_lambda(context, ast);
    case NECRO_AST_APATS_ASSIGNMENT:       return necro_ast_transform_to_core_apats_assignment(context, ast);
    case NECRO_AST_BIN_OP:                 return necro_ast_transform_to_core_bin_op(context, ast);
    case NECRO_AST_TUPLE:                  return necro_ast_transform_to_core_tuple(context, ast);

    // case NECRO_AST_IF_THEN_ELSE:
    //     return necro_transform_if_then_else(core_transform, necro_ast_node);

    // case NECRO_AST_PAT_ASSIGNMENT:
    //     return necro_transform_pat_assignment(core_transform, necro_ast_node);

    // case NECRO_AST_CASE:
    //     return necro_transform_case(core_transform, necro_ast_node);

    // TODO
    case NECRO_AST_PAT_EXPRESSION:
    case NECRO_AST_EXPRESSION_ARRAY:
    case NECRO_AST_ARITHMETIC_SEQUENCE:
    case NECRO_AST_OP_LEFT_SECTION:
    case NECRO_AST_OP_RIGHT_SECTION:
    case NECRO_AST_TYPE_CLASS_INSTANCE:
    case NECRO_AST_FOR_LOOP:
        assert(false && "TODO");
        return ok(NecroCoreAst, NULL);

    // Not Implemented
    case NECRO_AST_EXPRESSION_LIST:
    case NECRO_AST_DO:
    case NECRO_BIND_ASSIGNMENT:
        assert(false && "Not currently implemented, nor planned to be implemented");
        return ok(NecroCoreAst, NULL);

    // Never Encounter
    case NECRO_AST_WILDCARD:
    case NECRO_AST_TOP_DECL:
    case NECRO_AST_APATS:
    case NECRO_AST_LIST_NODE:
    case NECRO_AST_TYPE_APP:
    case NECRO_AST_BIN_OP_SYM:
    case NECRO_AST_CONSTRUCTOR:
    case NECRO_AST_SIMPLE_TYPE:
    case NECRO_AST_TYPE_CLASS_CONTEXT:
    case NECRO_AST_TYPE_CLASS_DECLARATION:
    case NECRO_AST_TYPE_SIGNATURE:
    case NECRO_AST_FUNCTION_TYPE:
    case NECRO_AST_DECL:
    case NECRO_AST_TYPE_ATTRIBUTE:
        assert(false && "Should never encounter this ast type");
        return ok(NecroCoreAst, NULL);

    default:
        assert(false && "Unrecognized ast type");
        return ok(NecroCoreAst, NULL);
    }
}

///////////////////////////////////////////////////////
// Pretty Printing
///////////////////////////////////////////////////////
#define NECRO_CORE_AST_INDENT 2

void necro_core_ast_pretty_print_go(NecroCoreAst* ast, size_t depth);

void necro_core_ast_pretty_print(NecroCoreAst* ast)
{
    necro_core_ast_pretty_print_go(ast, 0);
    printf("\n");
}

// TODO: Refactor
// void necro_core_ast_pretty_print_alts(NecroCoreAST_CaseAlt* alts, NecroSymTable* symtable, NecroIntern* intern, size_t depth)
// {
//     while (alts != NULL)
//     {
//         print_white_space(depth);
//         if (alts->altCon == NULL)
//             printf("_");
//         else
//             necro_core_ast_pretty_print_go(alts->altCon, symtable, intern, depth + NECRO_CORE_INDENT);
//         printf(" -> ");
//         necro_core_ast_pretty_print_go(alts->expr, symtable, intern, depth + NECRO_CORE_INDENT);
//         if (alts->next != NULL)
//             printf("\n");
//         alts = alts->next;
//     }
// }

void necro_core_ast_pretty_print_go(NecroCoreAst* ast, size_t depth)
{
    if (ast == NULL)
        return;
    switch (ast->ast_type)
    {
    case NECRO_CORE_AST_APP:
    {
        necro_core_ast_pretty_print_go(ast->app.expr1, depth);
        printf(" ");
        if (ast->app.expr2->ast_type == NECRO_CORE_AST_APP)
            printf("(");
        necro_core_ast_pretty_print_go(ast->app.expr2, depth);
        if (ast->app.expr2->ast_type == NECRO_CORE_AST_APP)
            printf(")");
        return;
    }
    case NECRO_CORE_AST_BIND:
    {
        if (depth == 0)
        {
            printf("%s :: ", ast->bind.ast_symbol->name->str);
            necro_type_print(ast->bind.ast_symbol->type);
            // printf("\n    (arity = %d, isRecursive = %s)\n", info->arity, ast->list.expr->bind.is_recursive ? "true" : "false");
            printf("\n");
            print_white_space(depth);
        }
        printf("%s ", ast->bind.ast_symbol->name->str);
        NecroCoreAst* lambdas = ast->bind.expr;
        while (lambdas->ast_type == NECRO_CORE_AST_LAM)
        {
            printf("%s ", lambdas->lambda.arg->var.ast_symbol->name->str);
            lambdas = lambdas->lambda.expr;
        }
        printf("= ");
        necro_core_ast_pretty_print_go(lambdas, depth + NECRO_CORE_AST_INDENT);
        return;
    }
    case NECRO_CORE_AST_LAM:
    {
        printf("\\%s -> ", ast->lambda.arg->var.ast_symbol->name->str);
        necro_core_ast_pretty_print_go(ast->lambda.expr, depth);
        return;
    }
    case NECRO_CORE_AST_CASE:
    {
        printf("case ");
        necro_core_ast_pretty_print_go(ast->case_expr.expr, depth);
        printf(" of\n");
        // TODO: Refactor
        // necro_core_ast_pretty_print_alts(ast->case_expr.alts, symtable, intern, depth);
        return;
    }
    case NECRO_CORE_AST_LET:
    {
        // printf("\n");
        print_white_space(depth);
        if (depth > 0)
            printf("let ");
        necro_core_ast_pretty_print_go(ast->let.bind, depth);
        // printf("\n");
        if (depth > 0)
            printf(" in\n");
        print_white_space(depth);
        necro_core_ast_pretty_print_go(ast->let.expr, depth);
        // printf("\n");
        return;
    }
    case NECRO_CORE_AST_VAR:
    {
        printf("%s", ast->var.ast_symbol->name->str);
        return;
    }
    case NECRO_CORE_AST_LIT:
    {
        switch (ast->lit.type)
        {
        case NECRO_AST_CONSTANT_INTEGER:         printf("%lld", ast->lit.int_literal);       return;
        case NECRO_AST_CONSTANT_INTEGER_PATTERN: printf("%lld", ast->lit.int_literal);       return;
        case NECRO_AST_CONSTANT_FLOAT:           printf("%f", ast->lit.float_literal);       return;
        case NECRO_AST_CONSTANT_FLOAT_PATTERN:   printf("%f", ast->lit.float_literal);       return;
        case NECRO_AST_CONSTANT_CHAR:            printf("%c", ast->lit.char_literal);        return;
        case NECRO_AST_CONSTANT_STRING:          printf("%s", ast->lit.string_literal->str); return;
        default:                                                                             return;
        }
        return;
    }
    case NECRO_CORE_AST_DATA_DECL:
    {
        printf("%s :: ", ast->data_decl.ast_symbol->name->str);
        necro_type_print(ast->data_decl.ast_symbol->type->kind);
        printf(" where\n");
        NecroCoreAstList* cons = ast->data_decl.con_list;
        while (cons != NULL)
        {
            necro_core_ast_pretty_print_go(cons->data, depth + NECRO_CORE_AST_INDENT);
            printf("\n");
            cons = cons->next;
        }
        return;
    }
    case NECRO_CORE_AST_DATA_CON:
    {
        print_white_space(depth);
        printf("%s :: ", ast->data_con.ast_symbol->source_name->str);
        necro_type_print(ast->data_con.type);
        return;
    }
    // case NECRO_CORE_EXPR_LIST:
    // {
    //     while (ast != NULL)
    //     {
    //         if (ast->list.expr == NULL)
    //         {
    //             ast = ast->list.next;
    //             continue;
    //         }
    //         print_white_space(depth);
    //         necro_core_ast_pretty_print_go(ast->list.expr, depth);
    //         if (ast->list.expr->expr_type == NECRO_CORE_EXPR_BIND)
    //         {
    //             NecroSymbolInfo* info = necro_symtable_get(symtable, ast->list.expr->bind.var.id);
    //             printf("\n(arity = %d, isRecursive = %s, stateType = ", info->arity, ast->list.expr->bind.is_recursive ? "true" : "false");
    //             switch (info->state_type)
    //             {
    //             case NECRO_STATE_POLY:
    //                 printf(" Poly)");
    //                 break;
    //             case NECRO_STATE_CONSTANT:
    //                 printf(" Constant)");
    //                 break;
    //             case NECRO_STATE_POINTWISE:
    //                 printf(" Pointwise)");
    //                 break;
    //             case NECRO_STATE_STATEFUL:
    //                 printf(" Stateful)");
    //                 break;
    //             }
    //         }
    //         if (ast->list.next != NULL)
    //             printf("\n\n");
    //         else
    //             printf("\n");
    //         ast = ast->list.next;
    //     }
    //     return;
    // }
    default:
        assert(false && "Unimplemented AST type in necro_core_ast_pretty_print_go");
        return;
    }
}

#define NECRO_CORE_AST_VERBOSE 1
void necro_core_test_result(const char* test_name, const char* str, NECRO_RESULT_TYPE expected_result, const NECRO_RESULT_ERROR_TYPE* error_type)
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
    NecroResult(void) result = necro_ast_transform_to_core(info, &intern, &base, &ast, &core_ast);

    // Assert
#if NECRO_CORE_AST_VERBOSE
    // if (result.type == expected_result)
    //     necro_scoped_symtable_print_top_scopes(&scoped_symtable);
    // necro_ast_arena_print(&base.ast);
    // necro_ast_arena_print(&ast);
    printf("\n");
    necro_core_ast_pretty_print(core_ast.root);
    printf("\n");
#endif
    assert(result.type == expected_result);
    bool passed = result.type == expected_result;
    if (expected_result == NECRO_RESULT_ERROR)
    {
        assert(error_type != NULL);
        if (result.error != NULL && error_type != NULL)
        {
            assert(result.error->type == *error_type);
            passed &= result.error->type == *error_type;
        }
        else
        {
            passed = false;
        }
    }

    const char* result_string = passed ? "Passed" : "Failed";
    printf("Core %s test: %s\n", test_name, result_string);
    fflush(stdout);

    // Clean up
#if NECRO_CORE_AST_VERBOSE
    if (result.type == NECRO_RESULT_ERROR)
        necro_result_error_print(result.error, str, "Test");
    else if (result.error)
        necro_result_error_destroy(result.type, result.error);
#else
    necro_result_error_destroy(result.type, result.error);
#endif

    necro_core_ast_arena_destroy(&core_ast);
    necro_ast_arena_destroy(&ast);
    necro_base_destroy(&base);
    necro_parse_ast_arena_destroy(&parse_ast);
    necro_destroy_lex_token_vector(&tokens);
    necro_scoped_symtable_destroy(&scoped_symtable);
    necro_symtable_destroy(&symtable);
    necro_intern_destroy(&intern);
}

void necro_core_ast_test()
{
    necro_announce_phase("Core");
    // TODO: Need to set up Base to translate into core...but where/how?

    {
        const char* test_name   = "Basic 1";
        const char* test_source = ""
            "x = 0\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_core_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Basic 2";
        const char* test_source = ""
            "f x y = x || y\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_core_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Basic 3";
        const char* test_source = ""
            "data Jump = In | The | Fire\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_core_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Basic 4";
        const char* test_source = ""
            "data Polymorph a = Bear a | Wolf a | Hydra a a a\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_core_test_result(test_name, test_source, expect_error_result, NULL);
    }

}

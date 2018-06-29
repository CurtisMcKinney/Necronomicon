/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "case.h"
#include <ctype.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Transforms/Scalar.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassManagerBuilder.h>
#include "symtable.h"
#include "runtime/runtime.h"

/*
    TODO:
        * Constant equality
        * Redundant patterns error in case statements!
*/

//=====================================================
// Forward declaration
//=====================================================
struct NecroDecisionTree;
struct NecroDecisionTree* necro_maximal_sharing(struct NecroDecisionTree* tree);

//=====================================================
// Case
//=====================================================
// Based on:
//    * http://pauillac.inria.fr/~maranget/papers/ml05e-maranget.pdf
//    * https://www.cs.tufts.edu/~nr/pubs/match.pdf

typedef struct NecroPatternPath
{
    struct NecroPatternPath* parent;
    LLVMTypeRef              type;
    LLVMValueRef             value;
    size_t                   slot;
} NecroPatternPath;

typedef struct NecroPatternBinding
{
    NecroCoreAST_Expression*    var;
    NecroPatternPath*           path;
    struct NecroPatternBinding* next;
} NecroPatternBinding;

typedef enum
{
    NECRO_DECISION_TREE_LEAF,
    NECRO_DECISION_TREE_SWITCH,
    NECRO_DECISION_TREE_BIND,
} NECRO_DECISION_TREE_TYPE;

typedef struct
{
    NecroCoreAST_Expression* expression;
    NecroPatternBinding*     bindings;
} NecroDecisionTreeLeaf;

typedef struct
{
    NecroPatternPath*          path;
    struct NecroDecisionTree** cases;
    NecroCon*                  cons;
    size_t                     num_cases;
} NecroDecisionTreeSwitch;

typedef struct
{
    struct NecroDecisionTree* next;
    LLVMValueRef*             parent_var;
    size_t                    slot;
    LLVMValueRef              bound_var;
    NecroID*                  bound_ids;
    size_t                    num_bound_ids;
} NecroDecisionTreeBind;

typedef struct NecroDecisionTree
{
    union
    {
        NecroDecisionTreeSwitch tree_switch;
        NecroDecisionTreeLeaf   tree_leaf;
    };
    NECRO_DECISION_TREE_TYPE type;
    LLVMBasicBlockRef        block;
    size_t                   hash;
} NecroDecisionTree;

/*
    NecroPatternMatrix layout, R x C:

    pat[0][0] pat[0][1] pat[0][C] | -> bnd[0] & exp[0]
    pat[1][0] pat[1][1] pat[1][C] | -> bnd[1] & exp[1]
    pat[R][0] pat[R][1] pat[R][C] | -> bnd[R] & exp[R]
    -------------------------------
    pth[0]    pth[1]    pth[C]
*/

typedef struct NecroPatternMatrix
{
    NecroCoreAST_Expression*** patterns;
    NecroCoreAST_Expression**  expressions;
    NecroPatternBinding**      bindings;
    NecroPatternPath**         paths;
    size_t                     rows;
    size_t                     columns;
    NecroCon                   pattern_con;
    size_t                     con_num;
} NecroPatternMatrix;

NecroPatternBinding* necro_create_pattern_binding(NecroCodeGen* codegen, NecroCoreAST_Expression* var, NecroPatternPath* path, NecroPatternBinding* next)
{
    NecroPatternBinding* binding = necro_paged_arena_alloc(&codegen->arena, sizeof(NecroPatternBinding));
    binding->var                 = var;
    binding->path                = path;
    binding->next                = next;
    return binding;
}

NecroPatternPath* necro_create_pattern_path(NecroCodeGen* codegen, NecroPatternPath* parent, LLVMTypeRef type, LLVMValueRef value, size_t slot)
{
    NecroPatternPath* path = necro_paged_arena_alloc(&codegen->arena, sizeof(NecroPatternPath));
    path->parent           = parent;
    path->type             = type;
    path->value            = value;
    path->slot             = slot;
    return path;
}

NecroDecisionTree* necro_create_decision_tree_leaf(NecroCodeGen* codegen, NecroCoreAST_Expression* leaf_expression, NecroPatternBinding* bindings)
{
    NecroDecisionTree* leaf    = necro_paged_arena_alloc(&codegen->arena, sizeof(NecroDecisionTree));
    leaf->type                 = NECRO_DECISION_TREE_LEAF;
    leaf->tree_leaf.expression = leaf_expression;
    leaf->tree_leaf.bindings   = bindings;
    leaf->block                = NULL;
    leaf->hash                 = 0;
    return leaf;
}

NecroDecisionTree* necro_create_decision_tree_switch(NecroCodeGen* codegen, NecroPatternPath* path, NecroDecisionTree** cases, NecroCon* cons, size_t num_cases)
{
    NecroDecisionTree* tree_switch     = necro_paged_arena_alloc(&codegen->arena, sizeof(NecroDecisionTree));
    tree_switch->type                  = NECRO_DECISION_TREE_SWITCH;
    tree_switch->tree_switch.path      = path;
    tree_switch->tree_switch.cases     = cases;
    tree_switch->tree_switch.cons      = cons;
    tree_switch->tree_switch.num_cases = num_cases;
    tree_switch->block                 = NULL;
    tree_switch->hash                  = 0;
    return tree_switch;
}

NecroPatternMatrix necro_create_pattern_matrix(NecroCodeGen* codegen, NecroCon pattern_con, size_t rows, size_t columns)
{
    NecroCoreAST_Expression*** patterns    = necro_paged_arena_alloc(&codegen->arena, rows    * sizeof(NecroCoreAST_Expression**));
    NecroCoreAST_Expression**  expressions = necro_paged_arena_alloc(&codegen->arena, rows    * sizeof(NecroCoreAST_Expression*));
    NecroPatternBinding**      bindings    = necro_paged_arena_alloc(&codegen->arena, rows    * sizeof(NecroPatternBinding*));
    NecroPatternPath**         paths       = necro_paged_arena_alloc(&codegen->arena, columns * sizeof(NecroPatternPath*));
    for (size_t r = 0; r < rows; ++r)
    {
        patterns[r] = necro_paged_arena_alloc(&codegen->arena, columns * sizeof(NecroCoreAST_Expression*));
        for (size_t c = 0; c < columns; ++c)
        {
            patterns[r][c] = NULL;
            if (r == 0)
                paths[c] = NULL;
        }
        expressions[r] = NULL;
        bindings[r]    = NULL;
    }
    return (NecroPatternMatrix)
    {
        .rows        = rows,
        .columns     = columns,
        .pattern_con = pattern_con,
        .patterns    = patterns,
        .expressions = expressions,
        .bindings    = bindings,
        .paths       = paths,
        .con_num     = necro_symtable_get(codegen->symtable, pattern_con.id)->con_num,
    };
}

NecroPatternMatrix necro_specialize_matrix(NecroCodeGen* codegen, NecroPatternMatrix* matrix, NecroCon pattern_con)
{
    size_t     con_arity = 0;
    NecroType* con_type = necro_symtable_get(codegen->symtable, pattern_con.id)->type;
    assert(con_type != NULL);
    while (con_type->type == NECRO_TYPE_FOR)
        con_type = con_type->for_all.type;
    while (con_type->type == NECRO_TYPE_FUN)
    {
        con_arity++;
        con_type = con_type->fun.type2;
    }
    assert(con_type->type == NECRO_TYPE_CON);
    size_t new_rows    = 0;
    size_t new_columns = max(0, con_arity + (matrix->columns - 1));
    for (size_t r = 0; r < matrix->rows; ++r)
    {
        NecroCoreAST_Expression* row_head = matrix->patterns[r][0];
        if (row_head == NULL) // WildCard
        {
            new_rows++;
        }
        else if (row_head->expr_type == NECRO_CORE_EXPR_VAR) // Var
        {
            new_rows++;
        }
        else if (row_head->expr_type == NECRO_CORE_EXPR_DATA_CON) // Con
        {
            // Delete other constructors
            if (row_head->data_con.condid.id.id == pattern_con.id.id)
                new_rows++;
        }
        else
        {
            assert(false && "Pattern should be either data con, wild card, or constant!");
        }
    }
    NecroPatternMatrix specialized_matrix = necro_create_pattern_matrix(codegen, pattern_con, new_rows, new_columns);
    size_t             column_diff        = ((size_t)max(0, 1 + (((int32_t)new_columns) - ((int32_t)matrix->columns))));
    size_t             new_r              = 0;
    for (size_t r = 0; r < matrix->rows; ++r)
    {
        NecroCoreAST_Expression** row      = matrix->patterns[r];
        NecroCoreAST_Expression*  row_head = row[0];
        if (row_head == NULL || row_head->expr_type == NECRO_CORE_EXPR_VAR) // Wildcard / Var
        {
            specialized_matrix.bindings[new_r]    = matrix->bindings[r];
            for (size_t c_d = 0; c_d < column_diff; ++c_d)
            {
                specialized_matrix.patterns[new_r][c_d] = NULL;
                if (specialized_matrix.paths[c_d] == NULL)
                    specialized_matrix.paths[c_d] = necro_create_pattern_path(codegen, matrix->paths[0], necro_get_value_ptr(codegen), NULL, c_d);
            }
            for (size_t c = 0; c < matrix->columns - 1; ++c)
            {
                specialized_matrix.patterns[new_r][column_diff + c] = matrix->patterns[r][c + 1];
                if (specialized_matrix.paths[column_diff + c] == NULL)
                    specialized_matrix.paths[column_diff + c] = necro_create_pattern_path(codegen, matrix->paths[c + 1]->parent, matrix->paths[c + 1]->type, NULL, matrix->paths[c + 1]->slot);
            }
            specialized_matrix.expressions[new_r] = matrix->expressions[r];
        }
        else if (row_head->expr_type == NECRO_CORE_EXPR_DATA_CON) // Con
        {
            // Delete other constructors
            if (row_head->data_con.condid.id.id != pattern_con.id.id)
                continue;
            specialized_matrix.bindings[new_r]    = matrix->bindings[r];
            NecroCoreAST_Expression* con_args = matrix->patterns[r][0]->data_con.arg_list;
            for (size_t c_d = 0; c_d < column_diff; ++c_d)
            {
                assert(con_args != NULL);
                specialized_matrix.patterns[new_r][c_d] = con_args->list.expr;
                if (specialized_matrix.paths[c_d] == NULL)
                {
                    if (con_args->list.expr != NULL)
                    {
                        specialized_matrix.paths[c_d] = necro_create_pattern_path(codegen, matrix->paths[0], LLVMPointerType(necro_get_ast_llvm_type(codegen, con_args->list.expr), 0), NULL, c_d);
                    }
                    else
                    {
                        specialized_matrix.paths[c_d] = necro_create_pattern_path(codegen, matrix->paths[0], necro_get_value_ptr(codegen), NULL, c_d);
                    }
                }
                if (con_args->list.expr != NULL && con_args->list.expr->expr_type == NECRO_CORE_EXPR_VAR)
                {
                    specialized_matrix.bindings[new_r] = necro_create_pattern_binding(codegen, con_args->list.expr, specialized_matrix.paths[c_d], specialized_matrix.bindings[new_r]);
                }
                con_args = con_args->list.next;
            }
            for (size_t c = 0; c < matrix->columns - 1; ++c)
            {
                specialized_matrix.patterns[new_r][column_diff + c] = matrix->patterns[r][c + 1];
                if (specialized_matrix.paths[column_diff + c] == NULL)
                    specialized_matrix.paths[column_diff + c] = necro_create_pattern_path(codegen, matrix->paths[c + 1]->parent, matrix->paths[c + 1]->type, NULL, matrix->paths[c + 1]->slot);
            }
            specialized_matrix.expressions[new_r] = matrix->expressions[r];
        }
        else
        {
            assert(false && "Pattern should be either data con, wild card, or constant!");
        }
        new_r++;
    }
    for (size_t c = 0; c < new_columns; ++c)
    {
        assert(specialized_matrix.paths[c] != NULL);
    }
    return specialized_matrix;
}

NecroPatternMatrix necro_create_pattern_matrix_from_case(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroPatternPath* case_path)
{
    if (necro_is_codegen_error(codegen)) return (NecroPatternMatrix) { 0 };
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_CASE);
    assert(ast->case_expr.alts != NULL);
    assert(ast->case_expr.type != NULL);
    size_t rows    = 0;
    size_t columns = 1;
    NecroCoreAST_CaseAlt* alts = ast->case_expr.alts;
    while (alts != NULL)
    {
        rows++;
        alts = alts->next;
    }
    // Create and fill in matrix
    NecroPatternMatrix matrix    = necro_create_pattern_matrix(codegen, (NecroCon) { 0, 0 }, rows, columns);
    size_t             this_rows = 0;
    alts                         = ast->case_expr.alts;
    NecroPatternPath*  path      = necro_create_pattern_path(codegen, case_path, LLVMPointerType(necro_get_ast_llvm_type(codegen, ast->case_expr.expr), 0), case_path->value, 0);
    while (alts != NULL)
    {
        size_t this_columns = 1;
        if (alts->altCon == NULL) // Wildcard
        {
            matrix.patterns[this_rows][0] = NULL;
        }
        else if (alts->altCon->expr_type == NECRO_CORE_EXPR_VAR) // Var
        {
            matrix.patterns[this_rows][0] = NULL;
            matrix.bindings[this_rows]    = necro_create_pattern_binding(codegen, alts->altCon, path, matrix.bindings[this_rows]);
        }
        else if (alts->altCon->expr_type == NECRO_CORE_EXPR_DATA_CON) // Con
        {
            matrix.patterns[this_rows][0] = alts->altCon;
        }
        else
        {
            assert(false && "Case alternatives should be a constructor, wildcard, or constant!");
        }
        matrix.expressions[this_rows] = alts->expr;
        this_rows++;
        alts = alts->next;
    }
    matrix.paths[0] = path;
    return matrix;
}

NecroPatternMatrix* necro_drop_columns(NecroCodeGen* codegen, NecroPatternMatrix* matrix, size_t num_drop)
{
    // return matrix;
    if (necro_is_codegen_error(codegen)) return NULL;
    for (size_t r = 0; r < matrix->rows; ++r)
    {
        matrix->patterns[r] += num_drop;
    }
    matrix->paths   += num_drop;
    matrix->columns -= num_drop;
    return matrix;
}

NecroDecisionTree* necro_compile_pattern_matrix(NecroCodeGen* codegen, NecroPatternMatrix* matrix, NecroCoreAST_Expression* top_case_ast)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    if (matrix->rows == 0)
    {
        // TODO: Calculate string representation of missing pattern case!
        necro_throw_codegen_error(codegen, top_case_ast, "Non-exhaustive patterns in case statement!");
        return NULL;
    }
    bool all_wildcards = true;
    for (size_t c = 0; c < matrix->columns; ++c)
    {
        if (matrix->patterns[0][c] != NULL && matrix->patterns[0][c]->expr_type != NECRO_CORE_EXPR_VAR)
        {
            all_wildcards = false;
            break;
        }
    }
    if (all_wildcards)
        return necro_create_decision_tree_leaf(codegen, matrix->expressions[0], matrix->bindings[0]);
    // For now, doing simple Left-to-Right heuristic
    // Find pattern type and num_cons and drop all empty columns
    NecroType* pattern_type = NULL;
    size_t     num_cons     = 0;
    size_t     num_drop     = 0;
    for (size_t c = 0; c < matrix->columns; ++c)
    {
        for (size_t r = 0; r < matrix->rows; ++r)
        {
            if (matrix->patterns[r][c] == NULL)
            {
            }
            else if (matrix->patterns[r][c]->expr_type == NECRO_CORE_EXPR_VAR)
            {
            }
            else if (matrix->patterns[r][c]->expr_type == NECRO_CORE_EXPR_DATA_CON)
            {
                pattern_type = necro_symtable_get(codegen->symtable, matrix->patterns[r][c]->data_con.condid.id)->type;
                while (pattern_type->type == NECRO_TYPE_FOR)
                {
                    pattern_type = pattern_type->for_all.type;
                }
                while (pattern_type->type == NECRO_TYPE_FUN)
                {
                    pattern_type = pattern_type->fun.type2;
                }
                assert(pattern_type->type == NECRO_TYPE_CON);
                num_cons = necro_symtable_get(codegen->symtable, pattern_type->con.con.id)->con_num;
                goto necro_compile_pattern_matrix_post_drop;
            }
            else
            {
                assert(false);
            }
        }
        num_drop++;
    }
necro_compile_pattern_matrix_post_drop:
    matrix                        = necro_drop_columns(codegen, matrix, num_drop);
    NecroDecisionTree** cases     = necro_paged_arena_alloc(&codegen->arena, num_cons * sizeof(NecroDecisionTree*));
    NecroCon*           cons      = necro_paged_arena_alloc(&codegen->arena, num_cons * sizeof(NecroCon));
    NecroSymbolInfo*    info      = necro_symtable_get(codegen->symtable, pattern_type->con.con.id);
    NecroASTNode*       data_cons = info->ast->data_declaration.constructor_list;
    size_t              con_num   = 0;
    while (data_cons != NULL)
    {
        NecroCon pattern_con =
        {
            .id     = data_cons->list.item->constructor.conid->conid.id,
            .symbol = data_cons->list.item->constructor.conid->conid.symbol,
        };
        const char*        con_name   = necro_intern_get_string(codegen->intern, pattern_con.symbol);
        NecroPatternMatrix con_matrix = necro_specialize_matrix(codegen, matrix, pattern_con);             if (necro_is_codegen_error(codegen)) return NULL;
        cases[con_num]                = necro_compile_pattern_matrix(codegen, &con_matrix, top_case_ast);  if (necro_is_codegen_error(codegen)) return NULL;
        cons[con_num]                 = pattern_con;
        con_num++;
        data_cons = data_cons->list.next_item;
    }
    return necro_create_decision_tree_switch(codegen, matrix->paths[0], cases, cons, num_cons);
}

void necro_codegen_decision_tree(NecroCodeGen* codegen, NecroDecisionTree* tree, LLVMBasicBlockRef term_case_block, LLVMBasicBlockRef error_block, LLVMValueRef result_phi, NecroNodePrototype* outer)
{
    if (necro_is_codegen_error(codegen)) return;
    assert(codegen != NULL);
    assert(tree != NULL);
    assert(outer != NULL);
    assert(term_case_block != NULL);
    assert(result_phi != NULL);
    assert(tree->block == NULL);
    switch (tree->type)
    {
    case NECRO_DECISION_TREE_LEAF:
    {
        NecroPatternBinding* bindings = tree->tree_leaf.bindings;
        while (bindings != NULL)
        {
            // Load value
            LLVMValueRef offset_parent_ptr = necro_snapshot_gep(codegen, "bind_path_slot_ptr", bindings->path->parent->value, 2, (uint32_t[]) { 0, 1 + bindings->path->slot });
            LLVMValueRef value_ptr = LLVMBuildLoad(codegen->builder, offset_parent_ptr, "value_ptr");
            value_ptr = necro_maybe_cast(codegen, value_ptr, bindings->path->type);
            necro_symtable_get(codegen->symtable, bindings->var->var.id)->llvm_type      = bindings->path->type;
            necro_symtable_get(codegen->symtable, bindings->var->var.id)->llvm_value     = value_ptr;
            NecroNodePrototype* var_prototype = necro_create_necro_node_prototype(codegen, bindings->var->var, necro_intern_get_string(codegen->intern, bindings->var->var.symbol), bindings->path->type, bindings->path->type, outer, NECRO_NODE_STATELESS, false);
            necro_symtable_get(codegen->symtable, bindings->var->var.id)->node_prototype = var_prototype;
            bindings = bindings->next;
        }
        LLVMValueRef leaf_value = necro_calculate_node_call(codegen, tree->tree_leaf.expression, outer);
        if (necro_is_codegen_error(codegen)) return;
        LLVMBasicBlockRef current_block = LLVMGetInsertBlock(codegen->builder);
        LLVMAddIncoming(result_phi, &leaf_value, &current_block, 1);
        LLVMBuildBr(codegen->builder, term_case_block);
        break;
    }
    case NECRO_DECISION_TREE_SWITCH:
    {
        // Load value
        if (tree->tree_switch.path->value == NULL)
        {
            LLVMValueRef offset_parent_ptr = necro_snapshot_gep(codegen, "path_slot_ptr", tree->tree_switch.path->parent->value, 2, (uint32_t[]) { 0, 1 + tree->tree_switch.path->slot });
            LLVMValueRef value_ptr         = LLVMBuildLoad(codegen->builder, offset_parent_ptr, "value_ptr");
            tree->tree_switch.path->value  = necro_maybe_cast(codegen, value_ptr, tree->tree_switch.path->type);
        }
        if (tree->tree_switch.num_cases > 1)
        {
            LLVMValueRef con_ptr           = necro_snapshot_gep(codegen, "con_ptr", tree->tree_switch.path->value, 3, (uint32_t[]) { 0, 0, 1 });
            LLVMValueRef con_val           = LLVMBuildLoad(codegen->builder, con_ptr, "con_val");
            LLVMValueRef switch_value      = LLVMBuildSwitch(codegen->builder, con_val, error_block, tree->tree_switch.num_cases);
            for (size_t i = 0; i < tree->tree_switch.num_cases; ++i)
            {
                if (tree->tree_switch.cases[i]->block != NULL)
                {
                    LLVMAddCase(switch_value, LLVMConstInt(LLVMInt32TypeInContext(codegen->context), i, false), tree->tree_switch.cases[i]->block);
                }
                else
                {
                    const char*       case_name  = necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]) { necro_intern_get_string(codegen->intern, tree->tree_switch.cons[i].symbol), "_case" });
                    LLVMBasicBlockRef case_block = LLVMInsertBasicBlockInContext(codegen->context, term_case_block, case_name);
                    LLVMAddCase(switch_value, LLVMConstInt(LLVMInt32TypeInContext(codegen->context), i, false), case_block);
                    LLVMPositionBuilderAtEnd(codegen->builder, case_block);
                    necro_codegen_decision_tree(codegen, tree->tree_switch.cases[i], term_case_block, error_block, result_phi, outer);
                    if (necro_is_codegen_error(codegen)) return;
                    tree->tree_switch.cases[i]->block = case_block;
                }
            }
        }
        else
        {
            necro_codegen_decision_tree(codegen, tree->tree_switch.cases[0], term_case_block, error_block, result_phi, outer);
        }
        break;
    }
    }
}

void necro_print_decision_tree_go(NecroCodeGen* codegen, NecroDecisionTree* tree, size_t depth)
{
    if (necro_is_codegen_error(codegen)) return;
    assert(codegen != NULL);
    assert(tree != NULL);
    switch (tree->type)
    {
    case NECRO_DECISION_TREE_LEAF:
    {
        print_white_space(depth);
        printf("leaf: %p\n", tree);
        break;
    }
    case NECRO_DECISION_TREE_SWITCH:
    {
        print_white_space(depth);
        printf("tree: %p, slot: %d\n", tree, tree->tree_switch.path->slot);
        for (size_t i = 0; i < tree->tree_switch.num_cases; ++i)
        {

            print_white_space(depth + 2);
            printf("*%s:\n", necro_intern_get_string(codegen->intern, tree->tree_switch.cons[i].symbol));
            necro_print_decision_tree_go(codegen, tree->tree_switch.cases[i], depth + 4);
        }
        break;
    }
    }
}

void necro_print_decision_tree(NecroCodeGen* codegen, NecroDecisionTree* tree)
{
    necro_print_decision_tree_go(codegen, tree, 0);
}

LLVMValueRef necro_codegen_case(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_CASE);
    assert(ast->case_expr.alts != NULL);
    assert(ast->case_expr.type != NULL);
    LLVMValueRef       case_expr_value = necro_calculate_node_call(codegen, ast->case_expr.expr, outer); if (necro_is_codegen_error(codegen)) return NULL;
    NecroPatternPath*  case_path       = necro_create_pattern_path(codegen, NULL, LLVMPointerType(necro_get_ast_llvm_type(codegen, ast->case_expr.expr), 0), case_expr_value, 0);
    NecroPatternMatrix matrix          = necro_create_pattern_matrix_from_case(codegen, ast, case_path); if (necro_is_codegen_error(codegen)) return NULL;
    NecroDecisionTree* tree            = necro_compile_pattern_matrix(codegen, &matrix, ast);            if (necro_is_codegen_error(codegen)) return NULL;
    // necro_print_decision_tree(codegen, tree);
    tree                               = necro_maximal_sharing(tree);
    // necro_print_decision_tree(codegen, tree);
    // exit(0);
    LLVMBasicBlockRef  current_block   = LLVMGetInsertBlock(codegen->builder);
    LLVMBasicBlockRef  next_block      = LLVMGetNextBasicBlock(current_block);

    // End block
    LLVMBasicBlockRef term_case_block  = (next_block != NULL) ? LLVMInsertBasicBlock(next_block, "case_end") : LLVMAppendBasicBlock(codegen->current_func, "case_end");
    LLVMPositionBuilderAtEnd(codegen->builder, term_case_block);
    LLVMValueRef       case_result     = LLVMBuildPhi(codegen->builder, LLVMPointerType(necro_type_to_llvm_type(codegen, ast->case_expr.type, false), 0), "case_result");

    // Error block
    LLVMBasicBlockRef  err_block       = outer->case_error_block;
    if (err_block == NULL)
    {
        LLVMBasicBlockRef next_block = LLVMGetNextBasicBlock(term_case_block);
        err_block = (next_block == NULL) ? LLVMAppendBasicBlock(codegen->current_func, "case_error") : LLVMInsertBasicBlock(next_block, "case_error");
        LLVMPositionBuilderAtEnd(codegen->builder, err_block);
        LLVMValueRef exit_call = necro_build_call(codegen, codegen->runtime->functions.necro_error_exit, (LLVMValueRef[]) { LLVMConstInt(LLVMInt32TypeInContext(codegen->context), 1, false) }, 1, "");
        LLVMSetInstructionCallConv(exit_call, LLVMCCallConv);
        LLVMBuildUnreachable(codegen->builder);
        outer->case_error_block = err_block;
    }

    // Build case in current block
    LLVMPositionBuilderAtEnd(codegen->builder, current_block);
    necro_codegen_decision_tree(codegen, tree, term_case_block, err_block, case_result, outer);          if (necro_is_codegen_error(codegen)) return NULL;
    LLVMPositionBuilderAtEnd(codegen->builder, term_case_block);
    // LLVMViewFunctionCFG(codegen->current_func);
    return case_result;
}

void necro_calculate_node_prototype_case(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer_node)
{
    if (necro_is_codegen_error(codegen)) return;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_CASE);
    assert(ast->case_expr.alts != NULL);
    assert(ast->case_expr.type != NULL);
    necro_calculate_node_prototype(codegen, ast->case_expr.expr, outer_node);
    NecroCoreAST_CaseAlt* alts = ast->case_expr.alts;
    while (alts != NULL)
    {
        necro_calculate_node_prototype(codegen, alts->expr, outer_node);
        alts = alts->next;
    }
}

//=====================================================
// NecroDecisionTreeHashTable
// ----
// Linear probing hash table to convert a
// decision tree into a directed acyclic graph with
// maximal sharing by using cons hashing
//=====================================================
typedef struct
{
    NecroDecisionTree* tree;
    size_t             hash;
} NecroDecisionTreeHashTableNode;

typedef struct
{
    NecroDecisionTreeHashTableNode* buckets;
    size_t                          size;
    size_t                          count;
} NecroDecisionTreeHashTable;

bool necro_tree_is_equal(NecroDecisionTree* tree1, NecroDecisionTree* tree2)
{
    assert(tree1 != NULL);
    assert(tree2 != NULL);
    if (tree1->type != tree2->type)
        return false;
    switch (tree1->type)
    {
    case NECRO_DECISION_TREE_LEAF:
        return tree1->tree_leaf.expression == tree2->tree_leaf.expression;
    case NECRO_DECISION_TREE_SWITCH:
        if (tree1->tree_switch.num_cases != tree2->tree_switch.num_cases)
            return false;
        for (size_t i = 0; i < tree1->tree_switch.num_cases; ++i)
        {
            if (tree1->tree_switch.cons[i].id.id != tree2->tree_switch.cons[i].id.id)
                return false;
        }
        for (size_t i = 0; i < tree1->tree_switch.num_cases; ++i)
        {
            if (!necro_tree_is_equal(tree1->tree_switch.cases[i], tree2->tree_switch.cases[i]))
                return false;
        }
        return true;
    default:
        assert(false && "Unrecognized tree type in necro_tree_is_equal");
        return false;
    }
}

static size_t hash(size_t input)
{
    return (size_t)(input * 37);
}

size_t necro_tree_hash(NecroDecisionTree* tree)
{
    assert(tree != NULL);
    size_t h = 0;
    if (tree->hash != 0)
        return tree->hash;
    switch (tree->type)
    {
    case NECRO_DECISION_TREE_LEAF:
        h = hash((size_t) tree->tree_leaf.expression);
        break;
    case NECRO_DECISION_TREE_SWITCH:
        h = h ^ hash(tree->tree_switch.num_cases);
        for (size_t i = 0; i < tree->tree_switch.num_cases; ++i)
        {
            h = h ^ hash(tree->tree_switch.cons[i].id.id);
            h = h ^ necro_tree_hash(tree->tree_switch.cases[i]);
        }
        break;
    default:
        assert(false && "Unrecognized tree type in necro_tree_hash");
        break;
    }
    tree->hash = h;
    return h;
}

size_t necro_tree_count_nodes(NecroDecisionTree* tree)
{
    assert(tree != NULL);
    size_t count = 0;
    switch (tree->type)
    {
    case NECRO_DECISION_TREE_LEAF:
        count = 1;
        break;
    case NECRO_DECISION_TREE_SWITCH:
        count = 1;
        for (size_t i = 0; i < tree->tree_switch.num_cases; ++i)
        {
            count += necro_tree_count_nodes(tree->tree_switch.cases[i]);
        }
        break;
    default:
        assert(false && "Unrecognized tree type in necro_tree_count_nodes");
        break;
    }
    return count;
}

NecroDecisionTreeHashTable necro_create_decision_tree_hash_table(size_t initial_size)
{
    NecroDecisionTreeHashTable table;
    table.buckets = malloc(sizeof(NecroDecisionTreeHashTableNode) * initial_size * 2);
    table.count   = 0;
    table.size    = initial_size * 2;
    for (size_t i = 0; i < initial_size * 2; ++i)
    {
        table.buckets[i].hash = 0;
        table.buckets[i].tree = NULL;
    }
    return table;
}

void necro_destroy_decision_tree_hash_table(NecroDecisionTreeHashTable* table)
{
    if (table->buckets != NULL)
        free(table->buckets);
    table->count = 0;
    table->size  = 0;
}

void necro_decision_tree_grow(NecroDecisionTreeHashTable* table)
{
    assert(false && "necro_decision_tree_grow: Theoretically this is unneeded since we pre-compute the maximum table size. If you're seeing this, that assumption has been proven wrong!");
}

NecroDecisionTree* necro_decision_tree_insert(NecroDecisionTreeHashTable* table, NecroDecisionTree* tree)
{
    assert(table != NULL);
    assert(tree != NULL);
    if (table->count >= table->size * 2)
        necro_decision_tree_grow(table);
    size_t hash   = necro_tree_hash(tree);
    size_t bucket = hash & (table->size - 1);
    while (true)
    {
        if (table->buckets[bucket].tree != NULL && table->buckets[bucket].hash == hash && necro_tree_is_equal(tree, table->buckets[bucket].tree))
        {
            return table->buckets[bucket].tree;
        }
        else if (table->buckets[bucket].tree == NULL)
        {
            table->buckets[bucket].tree = tree;
            table->buckets[bucket].hash = hash;
            table->count++;
            return tree;
        }
        bucket = (bucket + 1) & (table->size - 1);
    }
}

NecroDecisionTree* necro_maximal_sharing_go(NecroDecisionTreeHashTable* table, NecroDecisionTree* tree)
{
    assert(table != NULL);
    assert(tree != NULL);
    switch (tree->type)
    {
    case NECRO_DECISION_TREE_LEAF:
        return necro_decision_tree_insert(table, tree);
    case NECRO_DECISION_TREE_SWITCH:
        for (size_t i = 0; i < tree->tree_switch.num_cases; ++i)
        {
            tree->tree_switch.cases[i] = necro_maximal_sharing_go(table, tree->tree_switch.cases[i]);
        }
        return necro_decision_tree_insert(table, tree);
    default:
        assert(false && "Unrecognized tree type in necro_maximal_sharing_go");
        return NULL;
    }
}

NecroDecisionTree* necro_maximal_sharing(NecroDecisionTree* tree)
{
    size_t                     size  = next_highest_pow_of_2(necro_tree_count_nodes(tree));
    NecroDecisionTreeHashTable table = necro_create_decision_tree_hash_table(size);
    necro_maximal_sharing_go(&table, tree);
    necro_destroy_decision_tree_hash_table(&table);
    return tree;
}
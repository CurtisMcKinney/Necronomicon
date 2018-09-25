/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "machine_case.h"
#include "symtable.h"
#include "machine_build.h"
#include "machine.h"
#include "core/core_create.h"

/*
    TODO:
        * Constant equality (more work required for non-Int constant equality)
        * Redundant patterns error in case statements!
*/

//=====================================================
// Forward declaration
//=====================================================
struct NecroDecisionTree;
struct NecroDecisionTree* necro_maximal_sharing(struct NecroDecisionTree* tree);
void necro_print_decision_tree(NecroMachineProgram* program, struct NecroDecisionTree* tree);

//=====================================================
// Case
//=====================================================
// Based on:
//    * http://pauillac.inria.fr/~maranget/papers/ml05e-maranget.pdf
//    * https://www.cs.tufts.edu/~nr/pubs/match.pdf

typedef struct NecroPatternPath
{
    struct NecroPatternPath* parent;
    NecroMachineType*        type;
    NecroMachineAST*         value;
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
    NECRO_DECISION_TREE_LIT_SWITCH,
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
    NecroPatternPath*          path;
    struct NecroDecisionTree** cases;
    NecroAstConstant* constants;
    size_t                     num_cases;
} NecroDecisionTreeLitSwitch;

typedef struct NecroDecisionTree
{
    union
    {
        NecroDecisionTreeSwitch    tree_switch;
        NecroDecisionTreeLitSwitch tree_lit_switch;
        NecroDecisionTreeLeaf      tree_leaf;
    };
    NECRO_DECISION_TREE_TYPE type;
    NecroMachineAST*         block;
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

NecroPatternBinding* necro_create_pattern_binding(NecroMachineProgram* program, NecroCoreAST_Expression* var, NecroPatternPath* path, NecroPatternBinding* next)
{
    NecroPatternBinding* binding = necro_paged_arena_alloc(&program->arena, sizeof(NecroPatternBinding));
    binding->var                 = var;
    binding->path                = path;
    binding->next                = next;
    return binding;
}

NecroPatternPath* necro_create_pattern_path(NecroMachineProgram* program, NecroPatternPath* parent, NecroMachineType* type, NecroMachineAST* value, size_t slot)
{
    NecroPatternPath* path = necro_paged_arena_alloc(&program->arena, sizeof(NecroPatternPath));
    path->parent           = parent;
    path->type             = type;
    path->value            = value;
    path->slot             = slot;
    return path;
}

NecroDecisionTree* necro_create_decision_tree_leaf(NecroMachineProgram* program, NecroCoreAST_Expression* leaf_expression, NecroPatternBinding* bindings)
{
    NecroDecisionTree* leaf    = necro_paged_arena_alloc(&program->arena, sizeof(NecroDecisionTree));
    leaf->type                 = NECRO_DECISION_TREE_LEAF;
    leaf->tree_leaf.expression = leaf_expression;
    leaf->tree_leaf.bindings   = bindings;
    leaf->block                = NULL;
    leaf->hash                 = 0;
    return leaf;
}

NecroDecisionTree* necro_create_decision_tree_switch(NecroMachineProgram* program, NecroPatternPath* path, NecroDecisionTree** cases, NecroCon* cons, size_t num_cases)
{
    NecroDecisionTree* tree_switch     = necro_paged_arena_alloc(&program->arena, sizeof(NecroDecisionTree));
    tree_switch->type                  = NECRO_DECISION_TREE_SWITCH;
    tree_switch->tree_switch.path      = path;
    tree_switch->tree_switch.cases     = cases;
    tree_switch->tree_switch.cons      = cons;
    tree_switch->tree_switch.num_cases = num_cases;
    tree_switch->block                 = NULL;
    tree_switch->hash                  = 0;
    return tree_switch;
}

NecroDecisionTree* necro_create_decision_tree_lit_switch(NecroMachineProgram* program, NecroPatternPath* path, NecroDecisionTree** cases, NecroAstConstant* constants, size_t num_cases)
{
    NecroDecisionTree* tree_switch         = necro_paged_arena_alloc(&program->arena, sizeof(NecroDecisionTree));
    tree_switch->type                      = NECRO_DECISION_TREE_LIT_SWITCH;
    tree_switch->tree_lit_switch.path      = path;
    tree_switch->tree_lit_switch.cases     = cases;
    tree_switch->tree_lit_switch.constants = constants;
    tree_switch->tree_lit_switch.num_cases = num_cases;
    tree_switch->block                     = NULL;
    tree_switch->hash                      = 0;
    return tree_switch;
}

NecroDecisionTree* necro_compile_pattern_matrix(NecroMachineProgram* program, NecroPatternMatrix* matrix, NecroCoreAST_Expression* top_case_ast);

NecroPatternMatrix necro_create_pattern_matrix(NecroMachineProgram* program, NecroCon pattern_con, size_t rows, size_t columns)
{
    NecroCoreAST_Expression*** patterns    = necro_paged_arena_alloc(&program->arena, rows    * sizeof(NecroCoreAST_Expression**));
    NecroCoreAST_Expression**  expressions = necro_paged_arena_alloc(&program->arena, rows    * sizeof(NecroCoreAST_Expression*));
    NecroPatternBinding**      bindings    = necro_paged_arena_alloc(&program->arena, rows    * sizeof(NecroPatternBinding*));
    NecroPatternPath**         paths       = necro_paged_arena_alloc(&program->arena, columns * sizeof(NecroPatternPath*));
    for (size_t r = 0; r < rows; ++r)
    {
        patterns[r] = necro_paged_arena_alloc(&program->arena, columns * sizeof(NecroCoreAST_Expression*));
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
        .con_num     = necro_symtable_get(program->symtable, pattern_con.id)->con_num,
    };
}

NecroPatternMatrix necro_create_pattern_matrix_from_case(NecroMachineProgram* program, NecroCoreAST_Expression* ast, NecroPatternPath* case_path)
{
    assert(program != NULL);
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
    NecroPatternMatrix matrix    = necro_create_pattern_matrix(program, (NecroCon) { 0, 0 }, rows, columns);
    size_t             this_rows = 0;
    alts                         = ast->case_expr.alts;
    NecroMachineType*  expr_type = necro_make_ptr_if_boxed(program, necro_type_to_machine_type(program, ast->case_expr.expr->necro_type));
    NecroPatternPath*  path      = necro_create_pattern_path(program, case_path, expr_type, case_path->value, 0);
    while (alts != NULL)
    {
        size_t this_columns = 1;
        UNUSED(this_columns);
        if (alts->altCon == NULL) // Wildcard
        {
            matrix.patterns[this_rows][0] = NULL;
        }
        else if (alts->altCon->expr_type == NECRO_CORE_EXPR_VAR) // Var
        {
            NecroSymbolInfo* info = necro_symtable_get(program->symtable, alts->altCon->var.id);
            if (info->is_constructor && info->arity == 0 && info->is_enum)
            {
                *alts->altCon = *necro_create_core_lit(&program->arena, (NecroAstConstant) { .type = NECRO_AST_CONSTANT_INTEGER_PATTERN, .int_literal = info->con_num });
                matrix.patterns[this_rows][0] = alts->altCon;
                path->type                    = program->necro_int_type;
            }
            else
            {
                matrix.patterns[this_rows][0] = NULL;
                matrix.bindings[this_rows]    = necro_create_pattern_binding(program, alts->altCon, path, matrix.bindings[this_rows]);
            }
        }
        else if (alts->altCon->expr_type == NECRO_CORE_EXPR_DATA_CON) // Con
        {
            NecroSymbolInfo* info = necro_symtable_get(program->symtable, alts->altCon->data_con.condid.id);
            if (info->is_constructor && info->arity == 0 && info->is_enum)
            {
                *alts->altCon = *necro_create_core_lit(&program->arena, (NecroAstConstant) { .type = NECRO_AST_CONSTANT_INTEGER_PATTERN, .int_literal = info->con_num });
                matrix.patterns[this_rows][0] = alts->altCon;
                path->type                    = program->necro_int_type;
            }
            else
            {
                matrix.patterns[this_rows][0] = alts->altCon;
            }
        }
        else if (alts->altCon->expr_type == NECRO_CORE_EXPR_LIT) // Literal
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

NecroPatternMatrix necro_specialize_matrix(NecroMachineProgram* program, NecroPatternMatrix* matrix, NecroCon pattern_con)
{
    size_t     con_arity = 0;
    NecroType* con_type = necro_symtable_get(program->symtable, pattern_con.id)->type;
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
    NecroPatternMatrix specialized_matrix = necro_create_pattern_matrix(program, pattern_con, new_rows, new_columns);
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
                    specialized_matrix.paths[c_d] = necro_create_pattern_path(program, matrix->paths[0], program->necro_poly_ptr_type, NULL, c_d);
            }
            for (size_t c = 0; c < matrix->columns - 1; ++c)
            {
                specialized_matrix.patterns[new_r][column_diff + c] = matrix->patterns[r][c + 1];
                if (specialized_matrix.paths[column_diff + c] == NULL)
                    specialized_matrix.paths[column_diff + c] = necro_create_pattern_path(program, matrix->paths[c + 1]->parent, matrix->paths[c + 1]->type, NULL, matrix->paths[c + 1]->slot);
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
                        specialized_matrix.paths[c_d] = necro_create_pattern_path(program, matrix->paths[0], necro_create_machine_ptr_type(&program->arena, necro_core_pattern_type_to_machine_type(program, con_args->list.expr)), NULL, c_d);
                    }
                    else
                    {
                        specialized_matrix.paths[c_d] = necro_create_pattern_path(program, matrix->paths[0], program->necro_poly_ptr_type, NULL, c_d);
                    }
                }
                if (con_args->list.expr != NULL && con_args->list.expr->expr_type == NECRO_CORE_EXPR_VAR)
                {
                    specialized_matrix.bindings[new_r] = necro_create_pattern_binding(program, con_args->list.expr, specialized_matrix.paths[c_d], specialized_matrix.bindings[new_r]);
                }
                con_args = con_args->list.next;
            }
            for (size_t c = 0; c < matrix->columns - 1; ++c)
            {
                specialized_matrix.patterns[new_r][column_diff + c] = matrix->patterns[r][c + 1];
                if (specialized_matrix.paths[column_diff + c] == NULL)
                    specialized_matrix.paths[column_diff + c] = necro_create_pattern_path(program, matrix->paths[c + 1]->parent, matrix->paths[c + 1]->type, NULL, matrix->paths[c + 1]->slot);
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

NecroPatternMatrix necro_specialize_lit_matrix(NecroMachineProgram* program, NecroPatternMatrix* matrix, NecroAstConstant lit)
{
    size_t new_rows    = 0;
    size_t new_columns = max(0, matrix->columns - 1);
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
        else if (row_head->expr_type == NECRO_CORE_EXPR_LIT) // Lit
        {
            // Delete other non-matching literals
            switch (row_head->lit.type)
            {
            case NECRO_AST_CONSTANT_FLOAT:
            case NECRO_AST_CONSTANT_FLOAT_PATTERN:
                if (row_head->lit.double_literal == lit.double_literal) // Floats not being handled very well here...
                    new_rows++;
                break;
            case NECRO_AST_CONSTANT_INTEGER:
            case NECRO_AST_CONSTANT_INTEGER_PATTERN:
                if (row_head->lit.int_literal == lit.int_literal)
                    new_rows++;
                break;
            case NECRO_AST_CONSTANT_CHAR:
            case NECRO_AST_CONSTANT_CHAR_PATTERN:
                if (row_head->lit.char_literal == lit.char_literal)
                    new_rows++;
                break;
            default:
                assert(false && "Found Non-Pattern literal while compiling case statement");
            }
        }
        else
        {
            assert(false && "Pattern should be literal constant");
        }
    }
    NecroPatternMatrix specialized_matrix = necro_create_pattern_matrix(program, (NecroCon) { 0, 0 }, new_rows, new_columns);
    // size_t             column_diff        = 1; // ((size_t)max(0, 1 + (((int32_t)new_columns) - ((int32_t)matrix->columns))));
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
                    specialized_matrix.paths[c_d] = necro_create_pattern_path(program, matrix->paths[0], program->necro_poly_ptr_type, NULL, c_d);
            }
            for (size_t c = 0; c < matrix->columns - 1; ++c)
            {
                specialized_matrix.patterns[new_r][column_diff + c] = matrix->patterns[r][c + 1];
                if (specialized_matrix.paths[column_diff + c] == NULL)
                    specialized_matrix.paths[column_diff + c] = necro_create_pattern_path(program, matrix->paths[c + 1]->parent, matrix->paths[c + 1]->type, NULL, matrix->paths[c + 1]->slot);
            }
            specialized_matrix.expressions[new_r] = matrix->expressions[r];
        }
        else if (lit.type == NECRO_AST_CONSTANT_STRING)
        {
            continue;
        }
        else if (row_head->expr_type == NECRO_CORE_EXPR_LIT) // Lit
        {
            assert(row_head->lit.type == lit.type);
            switch (lit.type)
            {
            case NECRO_AST_CONSTANT_INTEGER:
            case NECRO_AST_CONSTANT_INTEGER_PATTERN:
                if (row_head->lit.int_literal != lit.int_literal)
                    continue;
                break;
            case NECRO_AST_CONSTANT_FLOAT:
            case NECRO_AST_CONSTANT_FLOAT_PATTERN:
                if (row_head->lit.double_literal != lit.double_literal)
                    continue;
                break;
            case NECRO_AST_CONSTANT_CHAR:
            case NECRO_AST_CONSTANT_CHAR_PATTERN:
                if (row_head->lit.char_literal != lit.char_literal)
                    continue;
                break;
            default:
                assert(false && "Found Non-Pattern literal while compiling case statement");
            }
            specialized_matrix.bindings[new_r] = matrix->bindings[r];
            // NecroCoreAST_Expression* con_args = matrix->patterns[r][0]->data_con.arg_list;
            // for (size_t c_d = 0; c_d < column_diff; ++c_d)
            // {
            //     assert(con_args != NULL);
            // specialized_matrix.patterns[new_r][0] = matrix->patterns[r][0];
            // if (specialized_matrix.paths[0] == NULL)
            // {
            //     // if (con_args->list.expr != NULL)
            //     // {
            //     //     specialized_matrix.paths[c_d] = necro_create_pattern_path(program, matrix->paths[0], necro_create_machine_ptr_type(&program->arena, necro_core_pattern_type_to_machine_type(program, con_args->list.expr)), NULL, c_d);
            //     // }
            //     // else
            //     // {
            //     specialized_matrix.paths[0] = necro_create_pattern_path(program, matrix->paths[0], program->necro_poly_ptr_type, NULL, 0);
            //     // }
            // }
            //     if (con_args->list.expr != NULL && con_args->list.expr->expr_type == NECRO_CORE_EXPR_VAR)
            //     {
            //         specialized_matrix.bindings[new_r] = necro_create_pattern_binding(program, con_args->list.expr, specialized_matrix.paths[c_d], specialized_matrix.bindings[new_r]);
            //     }
            //     con_args = con_args->list.next;
            // }
            for (size_t c = 0; c < matrix->columns - 1; ++c)
            {
                specialized_matrix.patterns[new_r][column_diff + c] = matrix->patterns[r][c + 1];
                if (specialized_matrix.paths[column_diff + c] == NULL)
                    specialized_matrix.paths[column_diff + c] = necro_create_pattern_path(program, matrix->paths[c + 1]->parent, matrix->paths[c + 1]->type, NULL, matrix->paths[c + 1]->slot);
            }
            specialized_matrix.expressions[new_r] = matrix->expressions[r];
        }
        else
        {
            assert(false && "Pattern should be either data con, wild card, or constant!");
        }
        new_r++;
    }
    return specialized_matrix;
}

NecroPatternMatrix* necro_drop_columns(NecroPatternMatrix* matrix, size_t num_drop)
{
    // return matrix;
    // if (necro_is_codegen_error(codegen)) return NULL;
    for (size_t r = 0; r < matrix->rows; ++r)
    {
        matrix->patterns[r] += num_drop;
    }
    matrix->paths   += num_drop;
    matrix->columns -= num_drop;
    return matrix;
}

NecroDecisionTree* necro_finish_compile_literal_pattern_matrix(NecroMachineProgram* program, NecroPatternMatrix* matrix, NecroCoreAST_Expression* top_case_ast, size_t num_drop)
{
    size_t num_cases = 0;
    for (size_t i = 0; i < matrix->rows; ++i)
    {
        num_cases++;
        if (matrix->patterns[i][0] == NULL || matrix->patterns[i][0]->expr_type == NECRO_CORE_EXPR_VAR)
        {
            break;
        }
    }
    matrix                               = necro_drop_columns(matrix, num_drop);
    NecroDecisionTree**        cases     = necro_paged_arena_alloc(&program->arena, num_cases * sizeof(NecroDecisionTree*));
    NecroAstConstant* constants = necro_paged_arena_alloc(&program->arena, num_cases * sizeof(NecroAstConstant));
    for (size_t r = 0; r < matrix->rows; ++r)
    {
        if (matrix->patterns[r][0] == NULL || matrix->patterns[r][0]->expr_type == NECRO_CORE_EXPR_VAR)
        {
            NecroAstConstant lit = (NecroAstConstant) { .type = NECRO_AST_CONSTANT_STRING, .symbol = NULL };
            NecroPatternMatrix con_matrix = necro_specialize_lit_matrix(program, matrix, lit);
            cases[r]                      = necro_compile_pattern_matrix(program, &con_matrix, top_case_ast);
            constants[r]                  = lit;
            break;
        }
        else
        {
            assert(matrix->patterns[r][0]->expr_type == NECRO_CORE_EXPR_LIT);
            NecroPatternMatrix con_matrix = necro_specialize_lit_matrix(program, matrix, matrix->patterns[r][0]->lit);
            cases[r]                      = necro_compile_pattern_matrix(program, &con_matrix, top_case_ast);
            constants[r]                  = matrix->patterns[r][0]->lit;
        }
    }
    return necro_create_decision_tree_lit_switch(program, matrix->paths[0], cases, constants, num_cases);
}

NecroDecisionTree* necro_finish_compile_pattern_matrix(NecroMachineProgram* program, NecroPatternMatrix* matrix, NecroCoreAST_Expression* top_case_ast, size_t num_drop, size_t num_cons, NecroType* pattern_type)
{
    matrix                        = necro_drop_columns(matrix, num_drop);
    NecroDecisionTree** cases     = necro_paged_arena_alloc(&program->arena, num_cons * sizeof(NecroDecisionTree*));
    NecroCon*           cons      = necro_paged_arena_alloc(&program->arena, num_cons * sizeof(NecroCon));
    // TODO: Redo with new NecroAstSymbol system AND also, wtf why did a regular NecroAst sneak into here, shouldn't this be based off of the NecroCoreAst!?!?!?!
    NecroAstSymbol* data      = pattern_type->con.con_symbol;
    NecroAst*       data_cons = data->ast->data_declaration.constructor_list;
    size_t          con_num   = 0;
    while (data_cons != NULL)
    {
        NecroCon pattern_con =
        {
            .id     = data_cons->list.item->constructor.conid->conid.id,
            .symbol = data_cons->list.item->constructor.conid->conid.symbol,
        };
        NecroPatternMatrix con_matrix = necro_specialize_matrix(program, matrix, pattern_con);             // if (necro_is_codegen_error(codegen)) return NULL;
        cases[con_num]                = necro_compile_pattern_matrix(program, &con_matrix, top_case_ast);  // if (necro_is_codegen_error(codegen)) return NULL;
        cons[con_num]                 = pattern_con;
        con_num++;
        data_cons = data_cons->list.next_item;
    }
    return necro_create_decision_tree_switch(program, matrix->paths[0], cases, cons, num_cons);
}

NecroDecisionTree* necro_compile_pattern_matrix(NecroMachineProgram* program, NecroPatternMatrix* matrix, NecroCoreAST_Expression* top_case_ast)
{
    // if (necro_is_codegen_error(codegen)) return NULL;
    if (matrix->rows == 0)
    {
        // TODO: Calculate string representation of missing pattern case!
        // necro_throw_codegen_error(codegen, top_case_ast, "Non-exhaustive patterns in case statement!");
        assert(false && "Non-exhaustive patterns in case statement!");
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
        return necro_create_decision_tree_leaf(program, matrix->expressions[0], matrix->bindings[0]);
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
                pattern_type = necro_symtable_get(program->symtable, matrix->patterns[r][c]->data_con.condid.id)->type;
                while (pattern_type->type == NECRO_TYPE_FOR)
                {
                    pattern_type = pattern_type->for_all.type;
                }
                while (pattern_type->type == NECRO_TYPE_FUN)
                {
                    pattern_type = pattern_type->fun.type2;
                }
                assert(pattern_type->type == NECRO_TYPE_CON);
                num_cons = pattern_type->con.con_symbol->con_num;
                return necro_finish_compile_pattern_matrix(program, matrix, top_case_ast, num_drop, num_cons, pattern_type);
            }
            else if (matrix->patterns[r][c]->expr_type == NECRO_CORE_EXPR_LIT)
            {
                return necro_finish_compile_literal_pattern_matrix(program, matrix, top_case_ast, num_drop);
            }
            else
            {
                assert(false);
            }
        }
        num_drop++;
    }
    return necro_finish_compile_pattern_matrix(program, matrix, top_case_ast, num_drop, num_cons, pattern_type);
}

void necro_decision_tree_to_machine(NecroMachineProgram* program, NecroDecisionTree* tree, NecroMachineAST* term_case_block, NecroMachineAST* error_block, NecroMachineAST* result_phi, NecroMachineAST* outer)
{
    assert(program != NULL);
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
            NecroMachineAST* parent_val = necro_build_maybe_cast(program, outer->machine_def.update_fn, bindings->path->parent->value, bindings->path->parent->type);
            NecroMachineAST* value_ptr  = necro_build_load_from_slot(program, outer->machine_def.update_fn, parent_val, bindings->path->slot + 1, "value");
            necro_symtable_get(program->symtable, bindings->var->var.id)->necro_machine_ast = value_ptr;
            bindings = bindings->next;
        }
        NecroMachineAST* leaf_value   = necro_core_to_machine_3_go(program, tree->tree_leaf.expression, outer);
        NecroMachineAST* leaf_value_m = necro_build_maybe_cast(program, outer->machine_def.update_fn, leaf_value, result_phi->phi.result->necro_machine_type);
        NecroMachineAST* curr_block   = outer->machine_def.update_fn->fn_def._curr_block;
        necro_add_incoming_to_phi(program, outer->machine_def.update_fn, result_phi, curr_block, leaf_value_m);
        necro_build_break(program, outer->machine_def.update_fn, term_case_block);
        break;
    }
    case NECRO_DECISION_TREE_SWITCH:
    {
        // Load value
        if (tree->tree_switch.path->value == NULL)
        {
            NecroMachineAST* parent_val   = necro_build_maybe_cast(program, outer->machine_def.update_fn, tree->tree_switch.path->parent->value, tree->tree_switch.path->parent->type);
            tree->tree_switch.path->value = necro_build_load_from_slot(program, outer->machine_def.update_fn, parent_val, tree->tree_switch.path->slot + 1, "value");
        }
        if (tree->tree_switch.num_cases > 1)
        {
            NecroMachineAST*              val          = necro_build_maybe_cast(program, outer->machine_def.update_fn, tree->tree_switch.path->value, tree->tree_switch.path->type);
            NecroMachineAST*              tag          = necro_build_load_from_slot(program, outer->machine_def.update_fn, val, 0, "tag");
            struct NecroSwitchTerminator* switch_value = necro_build_switch(program, outer->machine_def.update_fn, tag, NULL, error_block);
            for (size_t i = 0; i < tree->tree_switch.num_cases; ++i)
            {
                if (tree->tree_switch.cases[i]->block != NULL)
                {
                    necro_add_case_to_switch(program, switch_value, tree->tree_switch.cases[i]->block, i);
                }
                else
                {
                    const char*      case_name  = necro_snapshot_arena_concat_strings(&program->snapshot_arena, 2, (const char*[]) { tree->tree_switch.cons[i].symbol->str, "_case" });
                    NecroMachineAST* case_block = necro_insert_block_before(program, outer->machine_def.update_fn, case_name, term_case_block);
                    necro_add_case_to_switch(program, switch_value, case_block, i);
                    necro_move_to_block(program, outer->machine_def.update_fn, case_block);
                    necro_decision_tree_to_machine(program, tree->tree_switch.cases[i], term_case_block, error_block, result_phi, outer);
                    tree->tree_switch.cases[i]->block = case_block;
                }
            }
        }
        else
        {
            necro_decision_tree_to_machine(program, tree->tree_switch.cases[0], term_case_block, error_block, result_phi, outer);
        }
        break;
    }
    case NECRO_DECISION_TREE_LIT_SWITCH:
    {
        // Load value
        if (tree->tree_lit_switch.path->value == NULL)
        {
            NecroMachineAST* parent_val       = necro_build_maybe_cast(program, outer->machine_def.update_fn, tree->tree_lit_switch.path->parent->value, tree->tree_lit_switch.path->parent->type);
            tree->tree_lit_switch.path->value = necro_build_load_from_slot(program, outer->machine_def.update_fn, parent_val, tree->tree_lit_switch.path->slot + 1, "value");
        }
        if (tree->tree_lit_switch.num_cases > 1)
        {
            // TODO: Need lit type in here!
            NecroMachineAST*              value        = necro_build_bit_cast(program, outer->machine_def.update_fn, tree->tree_lit_switch.path->value, program->necro_uint_type);
            // TODO / HACK: Assuming int for now...
            struct NecroSwitchTerminator* switch_value = necro_build_switch(program, outer->machine_def.update_fn, value, NULL, error_block);
            for (size_t i = 0; i < tree->tree_lit_switch.num_cases; ++i)
            {
                if (tree->tree_lit_switch.constants[i].type == NECRO_AST_CONSTANT_STRING)
                {
                    const char*      case_name  = "default";
                    NecroMachineAST* case_block = necro_insert_block_before(program, outer->machine_def.update_fn, case_name, term_case_block);
                    switch_value->else_block    = case_block;
                    // necro_add_case_to_switch(program, switch_value, case_block, (size_t) tree->tree_lit_switch.constants[i].int_literal);
                    necro_move_to_block(program, outer->machine_def.update_fn, case_block);
                    necro_decision_tree_to_machine(program, tree->tree_lit_switch.cases[i], term_case_block, error_block, result_phi, outer);
                    tree->tree_lit_switch.cases[i]->block = case_block;
                    break;
                }
                else if (tree->tree_lit_switch.cases[i]->block != NULL)
                {
                    necro_add_case_to_switch(program, switch_value, tree->tree_lit_switch.cases[i]->block, (size_t) tree->tree_lit_switch.constants[i].int_literal);
                }
                else
                {
                    char int_buf[20] = { 0 };
                    int64_t int_literal = tree->tree_lit_switch.constants[i].int_literal;
                    sprintf(int_buf, "%" PRId64, int_literal);
                    const char* int_buf_addr = (const char*) int_buf;
                    const char*      case_name  = necro_snapshot_arena_concat_strings(&program->snapshot_arena, 2, (const char*[]) { int_buf_addr, "_case" });
                    NecroMachineAST* case_block = necro_insert_block_before(program, outer->machine_def.update_fn, case_name, term_case_block);
                    necro_add_case_to_switch(program, switch_value, case_block, (size_t) tree->tree_lit_switch.constants[i].int_literal);
                    necro_move_to_block(program, outer->machine_def.update_fn, case_block);
                    necro_decision_tree_to_machine(program, tree->tree_lit_switch.cases[i], term_case_block, error_block, result_phi, outer);
                    tree->tree_lit_switch.cases[i]->block = case_block;
                }
            }
        }
        else
        {
            necro_decision_tree_to_machine(program, tree->tree_lit_switch.cases[0], term_case_block, error_block, result_phi, outer);
        }
        break;
    }
    }
}

///////////////////////////////////////////////////////
// Core to Machine
///////////////////////////////////////////////////////
void necro_core_to_machine_1_case(NecroMachineProgram* program, NecroCoreAST_Expression* ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_CASE);
    assert(ast->case_expr.alts != NULL);
    assert(ast->case_expr.type != NULL);
    necro_core_to_machine_1_go(program, ast->case_expr.expr, outer);
    NecroCoreAST_CaseAlt* alts = ast->case_expr.alts;
    while (alts != NULL)
    {
        necro_core_to_machine_1_go(program, alts->expr, outer);
        alts = alts->next;
    }
}

void necro_core_to_machine_2_case(NecroMachineProgram* program, NecroCoreAST_Expression* ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_CASE);
    assert(ast->case_expr.alts != NULL);
    assert(ast->case_expr.type != NULL);
    necro_core_to_machine_2_go(program, ast->case_expr.expr, outer);
    NecroCoreAST_CaseAlt* alts = ast->case_expr.alts;
    while (alts != NULL)
    {
        necro_core_to_machine_2_go(program, alts->expr, outer);
        alts = alts->next;
    }
}

NecroMachineAST* necro_core_to_machine_3_case(NecroMachineProgram* program, NecroCoreAST_Expression* ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_CASE);
    assert(ast->case_expr.alts != NULL);
    assert(ast->case_expr.type != NULL);
    NecroMachineAST*   case_expr_value = necro_core_to_machine_3_go(program, ast->case_expr.expr, outer);
    NecroPatternPath*  case_path       = necro_create_pattern_path(program, NULL, necro_make_ptr_if_boxed(program, necro_core_ast_to_machine_type(program, ast->case_expr.expr)), case_expr_value, 0);
    NecroPatternMatrix matrix          = necro_create_pattern_matrix_from_case(program, ast, case_path);
    NecroDecisionTree* tree            = necro_compile_pattern_matrix(program, &matrix, ast);
    // necro_print_decision_tree(program, tree);
    tree                               = necro_maximal_sharing(tree);
    // necro_print_decision_tree(program, tree);
    // exit(0);
    NecroMachineAST*  current_block   = outer->machine_def.update_fn->fn_def._curr_block;
    NecroMachineAST*  next_block      = current_block->block.next_block;

    // End block
    NecroMachineAST*  term_case_block  = (next_block != NULL) ? necro_insert_block_before(program, outer->machine_def.update_fn, "case_end", next_block) : necro_append_block(program, outer->machine_def.update_fn, "case_end");
    necro_move_to_block(program, outer->machine_def.update_fn, term_case_block);
    NecroMachineAST*  case_result      = necro_build_phi(program, outer->machine_def.update_fn, necro_make_ptr_if_boxed(program, necro_type_to_machine_type(program, ast->case_expr.type)), NULL);
    // HACK
    NecroMachineAST*  result_phi       = outer->machine_def.update_fn->fn_def._curr_block->block.statements[outer->machine_def.update_fn->fn_def._curr_block->block.num_statements - 1];
    // result_phi->necro_machine_type     = case_result->necro_machine_type;

    // Error block
    NecroMachineAST* err_block = outer->machine_def.update_fn->fn_def._err_block;
    if (err_block == NULL)
    {
        NecroMachineAST* next_error_block = term_case_block->block.next_block;
        err_block = (next_error_block == NULL) ? necro_append_block(program, outer->machine_def.update_fn, "error") : necro_insert_block_before(program, outer->machine_def.update_fn, "error", next_error_block);
        necro_move_to_block(program, outer->machine_def.update_fn, err_block);
        necro_build_call(program, outer->machine_def.update_fn, necro_symtable_get(program->symtable, program->runtime._necro_error_exit.id)->necro_machine_ast->fn_def.fn_value, (NecroMachineAST*[]) { necro_create_uint32_necro_machine_value(program, 1) }, 1, NECRO_C_CALL, "");
        necro_build_unreachable(program, outer->machine_def.update_fn);
        outer->machine_def.update_fn->fn_def._err_block = err_block;
    }

    // Build case in current block
    necro_move_to_block(program, outer->machine_def.update_fn, current_block);
    necro_decision_tree_to_machine(program, tree, term_case_block, err_block, result_phi, outer);
    necro_move_to_block(program, outer->machine_def.update_fn, term_case_block);
    return case_result;
}

// TODO: Finish adding literal case statements!

//=====================================================
// NecroDecisionTreeHashTable
//-----------------------------------------------------
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
    case NECRO_DECISION_TREE_LIT_SWITCH:
        if (tree1->tree_lit_switch.num_cases != tree2->tree_lit_switch.num_cases)
            return false;
        for (size_t i = 0; i < tree1->tree_lit_switch.num_cases; ++i)
        {
            switch (tree1->tree_lit_switch.constants[i].type)
            {
            case NECRO_AST_CONSTANT_INTEGER:
            case NECRO_AST_CONSTANT_INTEGER_PATTERN:
                if (tree1->tree_lit_switch.constants[i].int_literal != tree2->tree_lit_switch.constants[i].int_literal)
                    return false;
                break;
            case NECRO_AST_CONSTANT_FLOAT:
            case NECRO_AST_CONSTANT_FLOAT_PATTERN:
                if (tree1->tree_lit_switch.constants[i].double_literal != tree2->tree_lit_switch.constants[i].double_literal)
                    return false;
                break;
            case NECRO_AST_CONSTANT_CHAR:
            case NECRO_AST_CONSTANT_CHAR_PATTERN:
                if (tree1->tree_lit_switch.constants[i].char_literal != tree2->tree_lit_switch.constants[i].char_literal)
                    return false;
                break;
            case NECRO_AST_CONSTANT_STRING:
                return true;
                // return false;
                break;
            }
        }
        for (size_t i = 0; i < tree1->tree_lit_switch.num_cases; ++i)
        {
            if (!necro_tree_is_equal(tree1->tree_lit_switch.cases[i], tree2->tree_lit_switch.cases[i]))
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
    case NECRO_DECISION_TREE_LIT_SWITCH:
        h = h ^ hash(tree->tree_lit_switch.num_cases);
        for (size_t i = 0; i < tree->tree_lit_switch.num_cases; ++i)
        {
            if (tree->tree_lit_switch.constants[i].type == NECRO_AST_CONSTANT_STRING)
                h = h ^ hash(31);
            else
                h = h ^ hash((size_t)tree->tree_lit_switch.constants[i].int_literal);
            h = h ^ necro_tree_hash(tree->tree_lit_switch.cases[i]);
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
    case NECRO_DECISION_TREE_LIT_SWITCH:
        count = 1;
        for (size_t i = 0; i < tree->tree_lit_switch.num_cases; ++i)
        {
            count += necro_tree_count_nodes(tree->tree_lit_switch.cases[i]);
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
    UNUSED(table);
    assert(false && "necro_decision_tree_grow: Theoretically this is unneeded since we pre-compute the maximum table size. If you're seeing this, something has gone wrong!");
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
    case NECRO_DECISION_TREE_LIT_SWITCH:
        for (size_t i = 0; i < tree->tree_lit_switch.num_cases; ++i)
        {
            tree->tree_lit_switch.cases[i] = necro_maximal_sharing_go(table, tree->tree_lit_switch.cases[i]);
        }
        return necro_decision_tree_insert(table, tree);
    default:
        assert(false && "Unrecognized tree type in necro_maximal_sharing_go");
        return NULL;
    }
}

NecroDecisionTree* necro_maximal_sharing(NecroDecisionTree* tree)
{
    size_t                     size  = (size_t)next_highest_pow_of_2((uint32_t)necro_tree_count_nodes(tree));
    NecroDecisionTreeHashTable table = necro_create_decision_tree_hash_table(size);
    necro_maximal_sharing_go(&table, tree);
    necro_destroy_decision_tree_hash_table(&table);
    return tree;
}

void necro_print_decision_tree_go(NecroMachineProgram* program, NecroDecisionTree* tree, size_t depth)
{
    // if (necro_is_codegen_error(codegen)) return;
    assert(program != NULL);
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
        printf("tree: %p, slot: %zu\n", tree, tree->tree_switch.path->slot);
        for (size_t i = 0; i < tree->tree_switch.num_cases; ++i)
        {

            print_white_space(depth + 2);
            printf("*%s:\n", tree->tree_switch.cons[i].symbol->str);
            necro_print_decision_tree_go(program, tree->tree_switch.cases[i], depth + 4);
        }
        break;
    }
    case NECRO_DECISION_TREE_LIT_SWITCH:
    {
        print_white_space(depth);
        printf("tree: %p, slot: %zu\n", tree, tree->tree_lit_switch.path->slot);
        for (size_t i = 0; i < tree->tree_lit_switch.num_cases; ++i)
        {

            print_white_space(depth + 2);
            switch (tree->tree_lit_switch.constants[i].type)
            {
            case NECRO_AST_CONSTANT_INTEGER:
            case NECRO_AST_CONSTANT_INTEGER_PATTERN:
                printf("*%lld:\n", tree->tree_lit_switch.constants[i].int_literal);
                break;
            case NECRO_AST_CONSTANT_FLOAT:
            case NECRO_AST_CONSTANT_FLOAT_PATTERN:
                printf("*%f:\n", tree->tree_lit_switch.constants[i].double_literal);
                break;
            case NECRO_AST_CONSTANT_CHAR:
            case NECRO_AST_CONSTANT_CHAR_PATTERN:
                printf("*%c:\n", tree->tree_lit_switch.constants[i].char_literal);
                break;
            case NECRO_AST_CONSTANT_STRING:
                printf("*_:\n");
                break;
            }
            necro_print_decision_tree_go(program, tree->tree_lit_switch.cases[i], depth + 4);
        }
        break;
    }
    default:
        assert(false);
        break;
    }
}

void necro_print_decision_tree(NecroMachineProgram* program, NecroDecisionTree* tree)
{
    necro_print_decision_tree_go(program, tree, 0);
}


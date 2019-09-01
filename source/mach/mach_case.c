/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "mach_case.h"
#include "symtable.h"
#include "utility/math.h"
#include "mach_type.h"
#include "mach_transform.h"

/*
    TODO:
        * Refactor
        * Constant equality (more work required for non-Int constant equality)
        * Redundant patterns error in case statements!
*/

//=====================================================
// Forward declaration
//=====================================================
struct NecroDecisionTree;
struct NecroDecisionTree* necro_decision_tree_maximal_sharing(struct NecroDecisionTree* tree);
void necro_decision_tree_print(NecroMachProgram* program, struct NecroDecisionTree* tree);

//=====================================================
// Case
//=====================================================
// Based on:
//    * http://pauillac.inria.fr/~maranget/papers/ml05e-maranget.pdf
//    * https://www.cs.tufts.edu/~nr/pubs/match.pdf

typedef enum
{
    NECRO_PATTERN_WILDCARD,
    NECRO_PATTERN_VAR,
    NECRO_PATTERN_APP,
    NECRO_PATTERN_CON,
    NECRO_PATTERN_ENUM,
    NECRO_PATTERN_LIT,
    NECRO_PATTERN_TOP,
} NECRO_PATTERN_TYPE;

typedef struct NecroPattern
{
    struct NecroPattern* parent;
    NecroMachType*       parent_con_type;
    size_t               parent_slot;
    NecroMachType*       value_type;
    NecroMachAst*        value_ast;
    NecroCoreAst*        pat_ast;
    NECRO_PATTERN_TYPE   pattern_type;
} NecroPattern;

typedef struct NecroPatternBinding
{
    // NecroCoreAst*               var;
    // NecroMachAstSymbol*         symbol;
    NecroPattern*               pattern;
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
    NecroCoreAst*        expression;
    NecroPatternBinding* bindings;
} NecroDecisionTreeLeaf;

typedef struct
{
    NecroPattern*              pattern;
    struct NecroDecisionTree** cases;
    NecroCoreAstSymbol**       symbols;
    size_t                     num_cases;
} NecroDecisionTreeSwitch;

typedef struct
{
    NecroPattern*              pattern;
    struct NecroDecisionTree** cases;
    NecroCoreAstLiteral*       constants;
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
    NecroMachAst*            block;
    size_t                   hash;
} NecroDecisionTree;

/*
    Revised NecroPatternMatrix layout, R x C:

    pat[0][0] pat[0][1] ... pat[0][C] | -> bnd[0] & exp[0]
    pat[1][0] pat[1][1] ... pat[1][C] | -> bnd[1] & exp[1]
    ...       ...           ...            ...      ...
    pat[R][0] pat[R][1] ... pat[R][C] | -> bnd[R] & exp[R]

*/

typedef struct NecroPatternMatrix
{
    NecroPattern***       patterns;
    NecroCoreAst**        expressions;
    NecroPatternBinding** bindings;
    size_t                rows;
    size_t                columns;
} NecroPatternMatrix;

NecroPatternBinding* necro_pattern_binding_create(NecroMachProgram* program, NecroPattern* pattern, NecroPatternBinding* next)
{
    NecroPatternBinding* binding = necro_paged_arena_alloc(&program->arena, sizeof(NecroPatternBinding));
    binding->pattern             = pattern;
    binding->next                = next;
    return binding;
}

NecroCoreAst* necro_core_ast_unwrap_apps(NecroCoreAst* ast)
{
    assert(ast != NULL);
    while (ast->ast_type == NECRO_CORE_AST_APP)
        ast = ast->app.expr1;
    assert(ast->ast_type == NECRO_CORE_AST_VAR);
    return ast;
}

NecroPattern* necro_pattern_alloc(NecroMachProgram* program, NecroPattern* parent, NecroMachType* parent_con_type, size_t parent_slot, NecroMachAst* value_ast, NecroMachType* value_type, NecroCoreAst* pat_ast, NECRO_PATTERN_TYPE pattern_type)
{
    assert(pat_ast != NULL);
    NecroPattern* pat    = necro_paged_arena_alloc(&program->arena, sizeof(NecroPattern));
    pat->parent          = parent;
    pat->parent_con_type = parent_con_type;
    pat->parent_slot     = parent_slot;
    pat->value_type      = value_type;
    pat->value_ast       = value_ast;
    pat->pat_ast         = pat_ast;
    pat->pattern_type    = pattern_type;
    return pat;
}

NecroPattern* necro_pattern_create(NecroMachProgram* program, NecroPattern* parent, NecroCoreAst* pat_ast, const size_t slot)
{
    assert(pat_ast != NULL);
    switch (pat_ast->ast_type)
    {
    case NECRO_CORE_AST_VAR:
    {
        NecroMachAstSymbol* symbol = pat_ast->var.ast_symbol->mach_symbol;
        if (symbol->is_enum)
        {
            NecroMachAst* enum_value = necro_mach_value_create_word_uint(program, symbol->con_num);
            return necro_pattern_alloc(program, parent, parent->value_type, slot, enum_value, program->type_cache.word_uint_type, pat_ast, NECRO_PATTERN_ENUM);
        }
        else if (symbol->is_constructor)
        {
            NecroMachType* con_type = necro_mach_type_create_ptr(&program->arena, necro_mach_type_from_necro_type(program, pat_ast->necro_type));
            return necro_pattern_alloc(program, parent, con_type, slot, NULL, symbol->mach_type, pat_ast, NECRO_PATTERN_CON);
        }
        else
        {
            return necro_pattern_alloc(program, parent, parent->value_type, slot, NULL, symbol->mach_type, pat_ast, NECRO_PATTERN_VAR);
        }
    }
    case NECRO_CORE_AST_APP:
    {
        NecroCoreAst*  unwrapped         = necro_core_ast_unwrap_apps(pat_ast);
        assert(unwrapped->var.ast_symbol->mach_symbol != NULL);
        NecroMachType* parent_con_type   = unwrapped->var.ast_symbol->mach_symbol->mach_type->fn_type.parameters[0];
        // NecroSymbol    parent_con_symbol = parent_con_type->ptr_type.element_type->struct_type.symbol->name;
        // NecroSymbol    type_symbol       = necro_type_find(pat_ast->necro_type)->con.con_symbol->name;
        // if (parent_con_symbol == type_symbol)
        //     return parent;
        // else
        return necro_pattern_alloc(program, parent, parent_con_type, slot, NULL, parent_con_type, pat_ast, NECRO_PATTERN_APP);
    }
    case NECRO_CORE_AST_LIT:
    {
        NecroMachAst* lit_value = necro_core_transform_to_mach_3_go(program, pat_ast, NULL);
        return necro_pattern_alloc(program, parent, parent->value_type, slot, lit_value, lit_value->necro_machine_type, pat_ast, NECRO_PATTERN_LIT);
    }
    default:
        assert(false);
        return NULL;
    }
}

NecroPattern* necro_pattern_create_top(NecroMachProgram* program, NecroCoreAst* pat_ast, NecroMachAst* outer)
{
    NecroMachAst*  value      = necro_core_transform_to_mach_3_go(program, pat_ast, outer);
    NecroMachType* value_type = necro_mach_type_make_ptr_if_boxed(program, necro_mach_type_from_necro_type(program, pat_ast->necro_type));
    return necro_pattern_alloc(program, NULL, NULL, 0, value, value_type, pat_ast, NECRO_PATTERN_TOP);
}

NecroPattern* necro_pattern_create_wildcard(NecroMachProgram* program, NecroPattern* parent)
{
    return necro_pattern_alloc(program, parent, NULL, 0, NULL, NULL, NULL, NECRO_PATTERN_WILDCARD);
}

NecroDecisionTree* necro_decision_tree_create_leaf(NecroMachProgram* program, NecroCoreAst* leaf_expression, NecroPatternBinding* bindings)
{
    NecroDecisionTree* leaf    = necro_paged_arena_alloc(&program->arena, sizeof(NecroDecisionTree));
    leaf->type                 = NECRO_DECISION_TREE_LEAF;
    leaf->tree_leaf.expression = leaf_expression;
    leaf->tree_leaf.bindings   = bindings;
    leaf->block                = NULL;
    leaf->hash                 = 0;
    return leaf;
}

NecroDecisionTree* necro_decision_tree_create_switch(NecroMachProgram* program, NecroPattern* pattern, NecroDecisionTree** cases, NecroCoreAstSymbol** symbols, size_t num_cases)
{
    NecroDecisionTree* tree_switch     = necro_paged_arena_alloc(&program->arena, sizeof(NecroDecisionTree));
    tree_switch->type                  = NECRO_DECISION_TREE_SWITCH;
    tree_switch->tree_switch.pattern   = pattern;
    tree_switch->tree_switch.cases     = cases;
    tree_switch->tree_switch.symbols   = symbols;
    tree_switch->tree_switch.num_cases = num_cases;
    tree_switch->block                 = NULL;
    tree_switch->hash                  = 0;
    return tree_switch;
}

NecroDecisionTree* necro_decision_tree_create_lit_switch(NecroMachProgram* program, NecroPattern* pattern, NecroDecisionTree** cases, NecroCoreAstLiteral* constants, size_t num_cases)
{
    NecroDecisionTree* tree_switch         = necro_paged_arena_alloc(&program->arena, sizeof(NecroDecisionTree));
    tree_switch->type                      = NECRO_DECISION_TREE_LIT_SWITCH;
    tree_switch->tree_lit_switch.pattern   = pattern;
    tree_switch->tree_lit_switch.cases     = cases;
    tree_switch->tree_lit_switch.constants = constants;
    tree_switch->tree_lit_switch.num_cases = num_cases;
    tree_switch->block                     = NULL;
    tree_switch->hash                      = 0;
    return tree_switch;
}

NecroDecisionTree* necro_pattern_matrix_compile(NecroMachProgram* program, NecroPatternMatrix* matrix, NecroCoreAst* top_case_ast);

NecroPatternMatrix necro_pattern_matrix_create(NecroMachProgram* program, size_t rows, size_t columns)
{
    NecroPattern***       patterns    = necro_paged_arena_alloc(&program->arena, rows * sizeof(NecroPattern*));
    NecroCoreAst**        expressions = necro_paged_arena_alloc(&program->arena, rows * sizeof(NecroCoreAst*));
    NecroPatternBinding** bindings    = necro_paged_arena_alloc(&program->arena, rows * sizeof(NecroPatternBinding*));
    for (size_t r = 0; r < rows; ++r)
    {
        patterns[r] = necro_paged_arena_alloc(&program->arena, columns * sizeof(NecroCoreAst*));
        for (size_t c = 0; c < columns; ++c)
        {
            patterns[r][c] = NULL;
        }
        expressions[r] = NULL;
        bindings[r]    = NULL;
    }
    return (NecroPatternMatrix)
    {
        .rows           = rows,
        .columns        = columns,
        .patterns       = patterns,
        .expressions    = expressions,
        .bindings       = bindings,
    };
}

NecroPatternMatrix necro_pattern_matrix_create_from_case(NecroMachProgram* program, NecroCoreAst* ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(ast != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_CASE);
    assert(ast->case_expr.alts != NULL);
    assert(ast->case_expr.expr->necro_type != NULL);

    NecroPattern*     case_expr_pattern = necro_pattern_create_top(program, ast->case_expr.expr, outer);
    size_t            rows              = 0;
    size_t            columns           = 1;
    NecroCoreAstList* alts              = ast->case_expr.alts;
    while (alts != NULL)
    {
        rows++;
        alts = alts->next;
    }
    // Create and fill in matrix
    NecroPatternMatrix matrix      = necro_pattern_matrix_create(program, rows, columns);
    size_t             this_rows   = 0;
    alts                           = ast->case_expr.alts;
    while (alts != NULL)
    {
        NecroCoreAst* alt = alts->data;
        assert(alt->case_alt.pat != NULL);
        matrix.patterns[this_rows][0] = necro_pattern_create(program, case_expr_pattern, alt->case_alt.pat, 0);
        if (matrix.patterns[this_rows][0]->pattern_type == NECRO_PATTERN_VAR)
            matrix.bindings[this_rows] = necro_pattern_binding_create(program, matrix.patterns[this_rows][0], matrix.bindings[this_rows]);
        matrix.expressions[this_rows] = alt->case_alt.expr;
        this_rows++;
        alts = alts->next;
    }
    return matrix;
}

NecroPatternMatrix necro_pattern_matrix_specialize(NecroMachProgram* program, NecroPatternMatrix* matrix, NecroCoreAstSymbol* pattern_symbol, NecroType* data_con_necro_type)
{
    size_t con_arity   = necro_type_arity(data_con_necro_type);
    size_t new_rows    = 0;
    size_t new_columns = con_arity + (matrix->columns - 1);
    new_columns        = (new_columns == SIZE_MAX) ? 0 : new_columns;
    for (size_t r = 0; r < matrix->rows; ++r)
    {
        NecroPattern* row_head = matrix->patterns[r][0];
        assert(row_head != NULL);
        if (row_head->pattern_type == NECRO_PATTERN_WILDCARD || row_head->pattern_type == NECRO_PATTERN_VAR ||
           ((row_head->pattern_type == NECRO_PATTERN_CON || NECRO_PATTERN_APP == NECRO_PATTERN_APP) && necro_core_ast_unwrap_apps(row_head->pat_ast)->var.ast_symbol == pattern_symbol))
            new_rows++;
    }
    NecroPatternMatrix specialized_matrix = necro_pattern_matrix_create(program, new_rows, new_columns);
    size_t             column_diff        = ((size_t)MAX(0, (1 + (((int32_t)new_columns) - ((int32_t)matrix->columns)))));
    size_t             new_r              = 0;
    for (size_t r = 0; r < matrix->rows; ++r)
    {
        NecroPattern** row                 = matrix->patterns[r];
        NecroPattern*  row_head            = row[0];

        //--------------------
        // Var / Wildcard
        if (row_head->pattern_type == NECRO_PATTERN_WILDCARD || row_head->pattern_type == NECRO_PATTERN_VAR)
        {
            specialized_matrix.bindings[new_r] = matrix->bindings[r];
            for (size_t c_d = 0; c_d < column_diff; ++c_d)
            {
                specialized_matrix.patterns[new_r][c_d] = necro_pattern_create_wildcard(program, row_head->parent);
            }
            for (size_t c = 0; c < matrix->columns - 1; ++c)
            {
                specialized_matrix.patterns[new_r][column_diff + c] = matrix->patterns[r][c + 1];
            }
            specialized_matrix.expressions[new_r] = matrix->expressions[r];
        }

        //--------------------
        // Con / App
        else if (row_head->pattern_type == NECRO_PATTERN_APP || row_head->pattern_type == NECRO_PATTERN_CON)
        {
            NecroCoreAst* unwrapped = necro_core_ast_unwrap_apps(row_head->pat_ast);
            assert(unwrapped->var.ast_symbol->is_constructor);
            if (unwrapped->var.ast_symbol != pattern_symbol) // Delete other constructors
                continue;
            specialized_matrix.bindings[new_r] = matrix->bindings[r];
            NecroCoreAst* apps = row_head->pat_ast;
            for (size_t c_d_rev = 0; c_d_rev < column_diff; ++c_d_rev)
            {
                assert(apps->ast_type == NECRO_CORE_AST_APP);
                const size_t  c_d = (column_diff - 1) - c_d_rev;
                NecroCoreAst* pat = apps->app.expr2;
                specialized_matrix.patterns[new_r][c_d] = necro_pattern_create(program, row_head, pat, c_d);
                if (specialized_matrix.patterns[new_r][c_d]->pattern_type == NECRO_PATTERN_VAR)
                    specialized_matrix.bindings[new_r] = necro_pattern_binding_create(program, specialized_matrix.patterns[new_r][c_d], specialized_matrix.bindings[new_r]);
                apps = apps->app.expr1;
            }
            assert(unwrapped->ast_type == NECRO_CORE_AST_VAR);
            for (size_t c = 0; c < matrix->columns - 1; ++c)
            {
                specialized_matrix.patterns[new_r][column_diff + c] = matrix->patterns[r][c + 1];
            }
            specialized_matrix.expressions[new_r] = matrix->expressions[r];
        }

        else
        {
            assert(false);
        }
        new_r++;
    }
    return specialized_matrix;
}

/*
NecroPatternMatrix necro_pattern_matrix_specialize_lit(NecroMachProgram* program, NecroPatternMatrix* matrix, NecroCoreAstLiteral lit)
{
    size_t new_rows    = 0;
    size_t new_columns = matrix->columns - 1;
    new_columns        = new_columns == SIZE_MAX ? 0 : new_columns;
    for (size_t r = 0; r < matrix->rows; ++r)
    {
        NecroCoreAst* row_head = matrix->patterns[r][0];
        if (row_head == NULL) // WildCard
        {
            new_rows++;
        }
        else if (row_head->ast_type == NECRO_CORE_AST_VAR) // Var
        {
            new_rows++;
        }
        else if (row_head->ast_type == NECRO_CORE_AST_LIT) // Lit
        {
            // Delete other non-matching literals
            switch (row_head->lit.type)
            {
            case NECRO_AST_CONSTANT_FLOAT:
            case NECRO_AST_CONSTANT_FLOAT_PATTERN:
                if (row_head->lit.float_literal == lit.float_literal) // Floats not being handled very well here...
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
    NecroPatternMatrix specialized_matrix = necro_pattern_matrix_create(program, new_rows, new_columns);
    // size_t             column_diff        = 1; // ((size_t)MAX(0, 1 + (((int32_t)new_columns) - ((int32_t)matrix->columns))));
    size_t             column_diff        = ((size_t)MAX(0, 1 + (((int32_t)new_columns) - ((int32_t)matrix->columns))));
    size_t             new_r              = 0;
    for (size_t r = 0; r < matrix->rows; ++r)
    {
        NecroCoreAst** row      = matrix->patterns[r];
        NecroCoreAst*  row_head = row[0];
        if (row_head == NULL || row_head->ast_type == NECRO_CORE_AST_VAR) // Wildcard / Var
        // if (row_head->ast_type == NECRO_CORE_AST_VAR) // Wildcard / Var
        {
            specialized_matrix.bindings[new_r]    = matrix->bindings[r];
            for (size_t c_d = 0; c_d < column_diff; ++c_d)
            {
                specialized_matrix.patterns[new_r][c_d] = NULL;
                if (specialized_matrix.paths[c_d] == NULL && row_head != NULL)
                    specialized_matrix.paths[c_d] = necro_pattern_path_create(program, matrix->paths[0], matrix->paths[0]->type, necro_mach_type_create_ptr(&program->arena, necro_mach_type_from_necro_type(program, row_head->necro_type)), NULL, c_d);
                // Wild card == NULL type?
                else if (specialized_matrix.paths[c_d] == NULL && row_head == NULL)
                    specialized_matrix.paths[c_d] = necro_pattern_path_create(program, matrix->paths[0], matrix->paths[0]->type, NULL, NULL, c_d);
            }
            for (size_t c = 0; c < matrix->columns - 1; ++c)
            {
                specialized_matrix.patterns[new_r][column_diff + c] = matrix->patterns[r][c + 1];
                if (specialized_matrix.paths[column_diff + c] == NULL)
                    specialized_matrix.paths[column_diff + c] = necro_pattern_path_create(program, matrix->paths[c + 1]->parent, matrix->paths[c + 1]->parent_con_type, matrix->paths[c + 1]->type, NULL, matrix->paths[c + 1]->slot);
            }
            specialized_matrix.expressions[new_r] = matrix->expressions[r];
        }
        else if (lit.type == NECRO_AST_CONSTANT_STRING)
        {
            continue;
        }
        else if (row_head->ast_type == NECRO_CORE_AST_LIT) // Lit
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
                if (row_head->lit.float_literal != lit.float_literal)
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
            // NecroCoreAst* con_args = matrix->patterns[r][0]->data_con.arg_list;
            // for (size_t c_d = 0; c_d < column_diff; ++c_d)
            // {
            //     assert(con_args != NULL);
            // specialized_matrix.patterns[new_r][0] = matrix->patterns[r][0];
            // if (specialized_matrix.paths[0] == NULL)
            // {
            //     // if (con_args->list.expr != NULL)
            //     // {
            //     //     specialized_matrix.paths[c_d] = necro_pattern_path_create(program, matrix->paths[0], necro_create_machine_ptr_type(&program->arena, necro_core_pattern_type_to_machine_type(program, con_args->list.expr)), NULL, c_d);
            //     // }
            //     // else
            //     // {
            //     specialized_matrix.paths[0] = necro_pattern_path_create(program, matrix->paths[0], program->necro_poly_ptr_type, NULL, 0);
            //     // }
            // }
            //     if (con_args->list.expr != NULL && con_args->list.expr->ast_type == NECRO_CORE_AST_VAR)
            //     {
            //         specialized_matrix.bindings[new_r] = necro_pattern_binding_create(program, con_args->list.expr, specialized_matrix.paths[c_d], specialized_matrix.bindings[new_r]);
            //     }
            //     con_args = con_args->list.next;
            // }
            for (size_t c = 0; c < matrix->columns - 1; ++c)
            {
                specialized_matrix.patterns[new_r][column_diff + c] = matrix->patterns[r][c + 1];
                if (specialized_matrix.paths[column_diff + c] == NULL)
                    specialized_matrix.paths[column_diff + c] = necro_pattern_path_create(program, matrix->paths[c + 1]->parent, matrix->paths[c + 1]->parent_con_type, matrix->paths[c + 1]->type, NULL, matrix->paths[c + 1]->slot);
            }
            specialized_matrix.expressions[new_r] = matrix->expressions[r];
        }
        else
        {
            assert(false && "Pattern should be either var, app, wild card, or constant!");
        }
        new_r++;
    }
    return specialized_matrix;
}
*/

NecroPatternMatrix* necro_pattern_matrix_drop_columns(NecroPatternMatrix* matrix, size_t num_drop)
{
    for (size_t r = 0; r < matrix->rows; ++r)
    {
        matrix->patterns[r] += num_drop;
    }
    matrix->columns -= num_drop;
    return matrix;
}

/*
NecroDecisionTree* necro_pattern_matrix_finish_compile_literal(NecroMachProgram* program, NecroPatternMatrix* matrix, NecroCoreAst* top_case_ast, size_t num_drop)
{
    size_t num_cases = 0;
    for (size_t i = 0; i < matrix->rows; ++i)
    {
        num_cases++;
        if (matrix->patterns[i][0] == NULL || matrix->patterns[i][0]->ast_type == NECRO_CORE_AST_VAR)
        {
            break;
        }
    }
    matrix                         = necro_pattern_matrix_drop_columns(matrix, num_drop);
    NecroDecisionTree**  cases     = necro_paged_arena_alloc(&program->arena, num_cases * sizeof(NecroDecisionTree*));
    NecroCoreAstLiteral* constants = necro_paged_arena_alloc(&program->arena, num_cases * sizeof(NecroCoreAstLiteral));
    for (size_t r = 0; r < matrix->rows; ++r)
    {
        if (matrix->patterns[r][0] == NULL || matrix->patterns[r][0]->ast_type == NECRO_CORE_AST_VAR)
        {
            NecroCoreAstLiteral lit        = (NecroCoreAstLiteral) { .type = NECRO_AST_CONSTANT_STRING, .string_literal = NULL };
            NecroPatternMatrix  con_matrix = necro_pattern_matrix_specialize_lit(program, matrix, lit);
            cases[r]                       = necro_pattern_matrix_compile(program, &con_matrix, top_case_ast);
            constants[r]                   = lit;
            break;
        }
        else
        {
            assert(matrix->patterns[r][0]->ast_type == NECRO_CORE_AST_LIT);
            NecroPatternMatrix con_matrix = necro_pattern_matrix_specialize_lit(program, matrix, matrix->patterns[r][0]->lit);
            cases[r]                      = necro_pattern_matrix_compile(program, &con_matrix, top_case_ast);
            constants[r]                  = matrix->patterns[r][0]->lit;
        }
    }
    return necro_decision_tree_create_lit_switch(program, matrix->paths[0], cases, constants, num_cases);
}
*/

size_t necro_core_ast_count_cons(NecroCoreAst* data_decl)
{
    assert(data_decl->ast_type == NECRO_CORE_AST_DATA_DECL);
    size_t            num_cons = 0;
    NecroCoreAstList* con_list = data_decl->data_decl.con_list;
    while (con_list != NULL)
    {
        num_cons++;
        con_list = con_list->next;
    }
    return num_cons;
}

NecroDecisionTree* necro_pattern_matrix_finish_compile(NecroMachProgram* program, NecroPattern* switch_pattern, NecroPatternMatrix* matrix, NecroCoreAst* top_case_ast, size_t num_drop, NecroType* pattern_type)
{
    pattern_type                     = necro_type_find(pattern_type);
    NecroCoreAstSymbol*  data_symbol = pattern_type->con.con_symbol->core_ast_symbol;
    const size_t num_cons            = necro_core_ast_count_cons(data_symbol->ast);
    matrix                           = necro_pattern_matrix_drop_columns(matrix, num_drop);
    NecroDecisionTree**  cases       = necro_paged_arena_alloc(&program->arena, num_cons * sizeof(NecroDecisionTree*));
    NecroCoreAstSymbol** symbols     = necro_paged_arena_alloc(&program->arena, num_cons * sizeof(NecroCoreAstSymbol*));
    size_t               con_num     = 0;
    NecroCoreAstList*    data_cons   = data_symbol->ast->data_decl.con_list;
    while (data_cons != NULL)
    {
        assert(con_num < num_cons);
        NecroCoreAstSymbol* con_symbol = data_cons->data->data_con.ast_symbol;
        NecroPatternMatrix  con_matrix = necro_pattern_matrix_specialize(program, matrix, con_symbol, data_cons->data->data_con.type);
        cases[con_num]                 = necro_pattern_matrix_compile(program, &con_matrix, top_case_ast);
        symbols[con_num]               = con_symbol;
        con_num++;
        data_cons = data_cons->next;
    }
    return necro_decision_tree_create_switch(program, switch_pattern, cases, symbols, num_cons);
}

NecroDecisionTree* necro_pattern_matrix_compile(NecroMachProgram* program, NecroPatternMatrix* matrix, NecroCoreAst* top_case_ast)
{
    if (matrix->rows == 0)
    {
        // TODO: Calculate string representation of missing pattern case!
        // necro_throw_codegen_error(codegen, top_case_ast, "Non-exhaustive patterns in case statement!");
        assert(false && "Non-exhaustive patterns in case statement!");
        return NULL;
    }
    bool all_wildcards = true;
    for (size_t c = 0; c < matrix->columns && all_wildcards; ++c)
    {
        all_wildcards = matrix->patterns[0][c]->pattern_type == NECRO_PATTERN_WILDCARD || matrix->patterns[0][c]->pattern_type == NECRO_PATTERN_VAR;
    }
    if (all_wildcards)
        return necro_decision_tree_create_leaf(program, matrix->expressions[0], matrix->bindings[0]);
    // For now, doing simple Left-to-Right heuristic
    // Find pattern type and num_cons and drop all empty columns
    NecroType* pattern_type = NULL;
    size_t     num_drop     = 0;
    for (size_t c = 0; c < matrix->columns; ++c)
    {
        for (size_t r = 0; r < matrix->rows; ++r)
        {
            if (matrix->patterns[r][c]->pattern_type == NECRO_PATTERN_CON || matrix->patterns[r][c]->pattern_type == NECRO_PATTERN_APP)
            {
                return necro_pattern_matrix_finish_compile(program, matrix->patterns[r][c]->parent, matrix, top_case_ast, num_drop, matrix->patterns[r][c]->pat_ast->necro_type);
            }
            else if (matrix->patterns[r][c]->pattern_type == NECRO_PATTERN_LIT || matrix->patterns[r][c]->pattern_type == NECRO_PATTERN_ENUM)
            {
                assert(false && "TODO: Refactor");
                // return necro_pattern_matrix_finish_compile_literal(program, matrix->patterns[r][c]->parent, matrix, top_case_ast, num_drop);
            }
            else
            {
                assert(false);
            }
        }
        num_drop++;
    }
    return necro_pattern_matrix_finish_compile(program, NULL, matrix, top_case_ast, num_drop, pattern_type);
}

void necro_pattern_binding_to_mach(NecroMachProgram* program, NecroPatternBinding* binding, NecroMachAst* outer)
{
    assert(binding->pattern->pattern_type == NECRO_PATTERN_VAR);

    // Downcast parent value if NULL
    NecroPattern* parent = binding->pattern->parent;
    if (parent->value_ast == NULL)
    {
        assert(parent->pattern_type == NECRO_PATTERN_APP);
        parent->value_ast = necro_mach_build_down_cast(program, outer->machine_def.update_fn, parent->parent->value_ast, parent->parent_con_type);
    }

    if (binding->pattern->pat_ast->var.ast_symbol->mach_symbol == NULL)
        binding->pattern->pat_ast->var.ast_symbol->mach_symbol = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, binding->pattern->pat_ast->var.ast_symbol);
    if (binding->pattern->value_ast == NULL)
    {
        NecroMachAst* slot_ptr      = necro_mach_build_gep(program, outer->machine_def.update_fn, binding->pattern->parent->value_ast, (uint32_t[]) { 0, binding->pattern->parent_slot + 1 }, 2, "value_ptr");
        binding->pattern->value_ast = necro_mach_build_load(program, outer->machine_def.update_fn, slot_ptr, "value");
    }
    if (binding->pattern->pat_ast->var.ast_symbol->mach_symbol->ast == NULL)
    {
        binding->pattern->pat_ast->var.ast_symbol->mach_symbol->ast       = binding->pattern->value_ast;
        binding->pattern->pat_ast->var.ast_symbol->mach_symbol->mach_type = binding->pattern->value_ast->necro_machine_type;
    }
}

NecroMachAst* necro_decision_tree_to_mach(NecroMachProgram* program, NecroDecisionTree* tree, NecroMachAst* term_case_block, NecroMachAst* error_block, NecroMachAst* result_phi, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(tree != NULL);
    assert(tree->block == NULL);
    assert(outer != NULL);
    if (result_phi != NULL)
    {
        assert(term_case_block != NULL);
        assert(result_phi != NULL);
    }
    switch (tree->type)
    {
    case NECRO_DECISION_TREE_LEAF:
    {
        NecroPatternBinding* bindings = tree->tree_leaf.bindings;
        while (bindings != NULL)
        {
            necro_pattern_binding_to_mach(program, bindings, outer);
            bindings = bindings->next;
        }
        NecroMachAst* leaf_value = necro_core_transform_to_mach_3_go(program, tree->tree_leaf.expression, outer);
        if (result_phi != NULL)
        {
            // Multiple cases
            NecroMachAst* curr_block = outer->machine_def.update_fn->fn_def._curr_block;
            necro_mach_add_incoming_to_phi(program, result_phi, curr_block, leaf_value);
            necro_mach_build_break(program, outer->machine_def.update_fn, term_case_block);
            return NULL;
        }
        else
        {
            // Single case
            return leaf_value;
        }
    }
    case NECRO_DECISION_TREE_SWITCH:
    {
        // Load value
        if (tree->tree_switch.pattern->value_ast == NULL)
        {
            NecroMachAst* parent_val             = necro_mach_build_down_cast(program, outer->machine_def.update_fn, tree->tree_switch.pattern->parent->value_ast, tree->tree_switch.pattern->parent_con_type);
            NecroMachAst* slot_ptr               = necro_mach_build_gep(program, outer->machine_def.update_fn, parent_val, (uint32_t[]) { 0, tree->tree_switch.pattern->parent_slot + 1 }, 2, "slot_ptr");
            tree->tree_switch.pattern->value_ast = necro_mach_build_load(program, outer->machine_def.update_fn, slot_ptr, "value");
        }
        if (tree->tree_switch.num_cases > 1)
        {
            NecroMachAst*              tag_ptr      = necro_mach_build_gep(program, outer->machine_def.update_fn, tree->tree_switch.pattern->value_ast, (uint32_t[]) { 0, 0 }, 2, "tag_ptr");
            NecroMachAst*              tag          = necro_mach_build_load(program, outer->machine_def.update_fn, tag_ptr, "tag");
            NecroMachSwitchTerminator* switch_value = necro_mach_build_switch(program, outer->machine_def.update_fn, tag, NULL, error_block);
            for (size_t i = 0; i < tree->tree_switch.num_cases; ++i)
            {
                if (tree->tree_switch.cases[i]->block != NULL)
                {
                    necro_mach_add_case_to_switch(program, switch_value, tree->tree_switch.cases[i]->block, i);
                }
                else
                {
                    const char*   case_name  = necro_snapshot_arena_concat_strings(&program->snapshot_arena, 2, (const char*[]) { tree->tree_switch.symbols[i]->name->str, "_case" });
                    NecroMachAst* case_block = necro_mach_block_insert_before(program, outer->machine_def.update_fn, case_name, term_case_block);
                    necro_mach_add_case_to_switch(program, switch_value, case_block, i);
                    necro_mach_block_move_to(program, outer->machine_def.update_fn, case_block);
                    necro_decision_tree_to_mach(program, tree->tree_switch.cases[i], term_case_block, error_block, result_phi, outer);
                    tree->tree_switch.cases[i]->block = case_block;
                }
            }
            return NULL;
        }
        else
        {
            return necro_decision_tree_to_mach(program, tree->tree_switch.cases[0], term_case_block, error_block, result_phi, outer);
        }
        // return NULL;
    }
    case NECRO_DECISION_TREE_LIT_SWITCH:
    {
        /*
        // Load value
        if (tree->tree_lit_switch.path->value == NULL)
        {
            // NecroMachAst* parent_val          = necro_mach_build_maybe_bit_cast(program, outer->machine_def.update_fn, tree->tree_lit_switch.path->parent->value, tree->tree_lit_switch.path->parent->type);
            NecroMachAst* parent_val          = necro_mach_build_down_cast(program, outer->machine_def.update_fn, tree->tree_lit_switch.path->parent->value, tree->tree_lit_switch.path->parent->type);
            NecroMachAst* value_ptr           = necro_mach_build_gep(program, outer->machine_def.update_fn, parent_val, (uint32_t[]) { 0, tree->tree_lit_switch.path->slot + 1 }, 2, "value_ptr");
            tree->tree_lit_switch.path->value = necro_mach_build_load(program, outer->machine_def.update_fn, value_ptr, "value");
        }
        if (tree->tree_lit_switch.num_cases > 1)
        {
            // uint or int !?
            // TODO: Need lit type in here!
            NecroMachAst*              value        = necro_mach_build_maybe_bit_cast(program, outer->machine_def.update_fn, tree->tree_lit_switch.path->value, program->type_cache.word_uint_type);
            // TODO / HACK: Assuming int for now...
            NecroMachSwitchTerminator* switch_value = necro_mach_build_switch(program, outer->machine_def.update_fn, value, NULL, error_block);
            for (size_t i = 0; i < tree->tree_lit_switch.num_cases; ++i)
            {
                if (tree->tree_lit_switch.constants[i].type == NECRO_AST_CONSTANT_STRING)
                {
                    // NOTE: WTF is up with using this to represent Wildcards / vars!?!?!
                    // assert(false && "TODO: String literal support in case statements");
                    const char*   case_name  = "default";
                    NecroMachAst* case_block = necro_mach_block_insert_before(program, outer->machine_def.update_fn, case_name, term_case_block);
                    switch_value->else_block    = case_block;
                    // // necro_add_case_to_switch(program, switch_value, case_block, (size_t) tree->tree_lit_switch.constants[i].int_literal);
                    necro_mach_block_move_to(program, outer->machine_def.update_fn, case_block);
                    necro_decision_tree_to_mach(program, tree->tree_lit_switch.cases[i], term_case_block, error_block, result_phi, outer);
                    tree->tree_lit_switch.cases[i]->block = case_block;
                    break;
                }
                else if (tree->tree_lit_switch.cases[i]->block != NULL)
                {
                    necro_mach_add_case_to_switch(program, switch_value, tree->tree_lit_switch.cases[i]->block, (size_t) tree->tree_lit_switch.constants[i].int_literal);
                }
                else
                {
                    char    int_buf[20] = { 0 };
                    int64_t int_literal = tree->tree_lit_switch.constants[i].int_literal;
                    sprintf(int_buf, "%" PRId64, int_literal);
                    const char* int_buf_addr = (const char*) int_buf;
                    const char* case_name    = necro_snapshot_arena_concat_strings(&program->snapshot_arena, 2, (const char*[]) { int_buf_addr, "_case" });
                    NecroMachAst* case_block = necro_mach_block_insert_before(program, outer->machine_def.update_fn, case_name, term_case_block);
                    necro_mach_add_case_to_switch(program, switch_value, case_block, (size_t) tree->tree_lit_switch.constants[i].int_literal);
                    necro_mach_block_move_to(program, outer->machine_def.update_fn, case_block);
                    necro_decision_tree_to_mach(program, tree->tree_lit_switch.cases[i], term_case_block, error_block, result_phi, outer);
                    tree->tree_lit_switch.cases[i]->block = case_block;
                }
            }
        }
        else
        {
            necro_decision_tree_to_mach(program, tree->tree_lit_switch.cases[0], term_case_block, error_block, result_phi, outer);
        }
        */
        return NULL;
    }
    default:
        assert(false && "Unhandled tree node type");
        return NULL;
    }
}

///////////////////////////////////////////////////////
// Core to Machine
///////////////////////////////////////////////////////
void necro_core_transform_to_mach_1_case(NecroMachProgram* program, NecroCoreAst* ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(ast != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_CASE);
    assert(ast->case_expr.alts != NULL);
    assert(ast->case_expr.expr->necro_type != NULL);
    necro_core_transform_to_mach_1_go(program, ast->case_expr.expr, outer);
    NecroCoreAstList* alts = ast->case_expr.alts;
    while (alts != NULL)
    {
        necro_core_transform_to_mach_1_go(program, alts->data->case_alt.expr, outer);
        alts = alts->next;
    }
}

void necro_core_transform_to_mach_2_case(NecroMachProgram* program, NecroCoreAst* ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(ast != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_CASE);
    assert(ast->case_expr.alts != NULL);
    assert(ast->case_expr.expr->necro_type != NULL);
    necro_core_transform_to_mach_2_go(program, ast->case_expr.expr, outer);
    NecroCoreAstList* alts = ast->case_expr.alts;
    while (alts != NULL)
    {
        necro_core_transform_to_mach_2_go(program, alts->data->case_alt.expr, outer);
        alts = alts->next;
    }
}

NecroMachAst* necro_core_transform_to_mach_3_case(NecroMachProgram* program, NecroCoreAst* ast, NecroMachAst* outer)
{
    assert(program != NULL);
    assert(ast != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_CASE);
    assert(ast->case_expr.alts != NULL);
    assert(ast->case_expr.expr->necro_type != NULL);
    NecroPatternMatrix matrix          = necro_pattern_matrix_create_from_case(program, ast, outer);
    NecroDecisionTree* tree            = necro_pattern_matrix_compile(program, &matrix, ast);
    necro_decision_tree_print(program, tree);
    tree                               = necro_decision_tree_maximal_sharing(tree);
    necro_decision_tree_print(program, tree);

    if ((tree->type == NECRO_DECISION_TREE_SWITCH && tree->tree_switch.num_cases > 1) || (tree->type == NECRO_DECISION_TREE_LIT_SWITCH && tree->tree_switch.num_cases > 1))
    {
        // Multiple cases
        NecroMachAst* current_block = outer->machine_def.update_fn->fn_def._curr_block;
        NecroMachAst* next_block    = current_block->block.next_block;

        // TODO: Don't add block machinery if we're simply loading things from a non-sum type in case

        // End block
        NecroMachAst*  term_case_block  = (next_block != NULL) ? necro_mach_block_insert_before(program, outer->machine_def.update_fn, "case_end", next_block) : necro_mach_block_append(program, outer->machine_def.update_fn, "case_end");
        necro_mach_block_move_to(program, outer->machine_def.update_fn, term_case_block);
        NecroMachType* result_type      = necro_mach_type_make_ptr_if_boxed(program, necro_mach_type_from_necro_type(program, ast->case_expr.alts->data->case_alt.expr->necro_type));
        NecroMachAst*  case_result      = necro_mach_build_phi(program, outer->machine_def.update_fn, result_type, NULL);
        // HACK
        NecroMachAst*  result_phi       = outer->machine_def.update_fn->fn_def._curr_block->block.statements[outer->machine_def.update_fn->fn_def._curr_block->block.num_statements - 1];

        // Error block
        NecroMachAst* err_block = outer->machine_def.update_fn->fn_def._err_block;
        if (err_block == NULL)
        {
            NecroMachAst* next_error_block = term_case_block->block.next_block;
            err_block = (next_error_block == NULL) ? necro_mach_block_append(program, outer->machine_def.update_fn, "error") : necro_mach_block_insert_before(program, outer->machine_def.update_fn, "error", next_error_block);
            necro_mach_block_move_to(program, outer->machine_def.update_fn, err_block);
            necro_mach_build_call(program, outer->machine_def.update_fn, program->runtime._necro_error_exit->ast->fn_def.fn_value, (NecroMachAst*[]) { necro_mach_value_create_uint32(program, 1) }, 1, NECRO_MACH_CALL_C, "");
            necro_mach_build_unreachable(program, outer->machine_def.update_fn);
            outer->machine_def.update_fn->fn_def._err_block = err_block;
        }

        // Build case in current block
        necro_mach_block_move_to(program, outer->machine_def.update_fn, current_block);
        necro_decision_tree_to_mach(program, tree, term_case_block, err_block, result_phi, outer);
        necro_mach_block_move_to(program, outer->machine_def.update_fn, term_case_block);
        return case_result;
    }
    else
    {
        // Single case
        return necro_decision_tree_to_mach(program, tree, NULL, NULL, NULL, outer);
    }
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

bool necro_decision_tree_is_equal(NecroDecisionTree* tree1, NecroDecisionTree* tree2)
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
            if (tree1->tree_switch.symbols[i] != tree2->tree_switch.symbols[i])
                return false;
        }
        for (size_t i = 0; i < tree1->tree_switch.num_cases; ++i)
        {
            if (!necro_decision_tree_is_equal(tree1->tree_switch.cases[i], tree2->tree_switch.cases[i]))
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
                if (tree1->tree_lit_switch.constants[i].float_literal != tree2->tree_lit_switch.constants[i].float_literal)
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
            default:
                assert(false && "Unhandled type");
                break;
            }
        }
        for (size_t i = 0; i < tree1->tree_lit_switch.num_cases; ++i)
        {
            if (!necro_decision_tree_is_equal(tree1->tree_lit_switch.cases[i], tree2->tree_lit_switch.cases[i]))
                return false;
        }
        return true;
    default:
        assert(false && "Unrecognized tree type in necro_decision_tree_is_equal");
        return false;
    }
}

static size_t hash(size_t input)
{
    return (size_t)(input * 37);
}

size_t necro_decision_tree_hash(NecroDecisionTree* tree)
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
            // h = h ^ hash(tree->tree_switch.symbols[i]);
            h = h ^ tree->tree_switch.symbols[i]->name->hash;
            h = h ^ necro_decision_tree_hash(tree->tree_switch.cases[i]);
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
            h = h ^ necro_decision_tree_hash(tree->tree_lit_switch.cases[i]);
        }
        break;
    default:
        assert(false && "Unrecognized tree type in necro_decision_tree_hash");
        break;
    }
    tree->hash = h;
    return h;
}

size_t necro_decision_tree_count_nodes(NecroDecisionTree* tree)
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
            count += necro_decision_tree_count_nodes(tree->tree_switch.cases[i]);
        }
        break;
    case NECRO_DECISION_TREE_LIT_SWITCH:
        count = 1;
        for (size_t i = 0; i < tree->tree_lit_switch.num_cases; ++i)
        {
            count += necro_decision_tree_count_nodes(tree->tree_lit_switch.cases[i]);
        }
        break;
    default:
        assert(false && "Unrecognized tree type in necro_decision_tree_count_nodes");
        break;
    }
    return count;
}

NecroDecisionTreeHashTable necro_decision_tree_hash_table_create(size_t initial_size)
{
    NecroDecisionTreeHashTable table;
    table.buckets = emalloc(sizeof(NecroDecisionTreeHashTableNode) * initial_size * 2);
    table.count   = 0;
    table.size    = initial_size * 2;
    for (size_t i = 0; i < initial_size * 2; ++i)
    {
        table.buckets[i].hash = 0;
        table.buckets[i].tree = NULL;
    }
    return table;
}

void necro_decision_tree_hash_table_destroy(NecroDecisionTreeHashTable* table)
{
    if (table->buckets != NULL)
        free(table->buckets);
    table->count = 0;
    table->size  = 0;
}

void necro_decision_tree_hash_table_grow(NecroDecisionTreeHashTable* table)
{
    UNUSED(table);
    assert(false && "necro_decision_tree_hash_table_grow: Theoretically this is unneeded since we pre-compute the maximum table size. If you're seeing this, something has gone wrong!");
}

NecroDecisionTree* necro_decision_tree_hash_table_insert(NecroDecisionTreeHashTable* table, NecroDecisionTree* tree)
{
    assert(table != NULL);
    assert(tree != NULL);
    if (table->count >= table->size * 2)
        necro_decision_tree_hash_table_grow(table);
    size_t hash   = necro_decision_tree_hash(tree);
    size_t bucket = hash & (table->size - 1);
    while (true)
    {
        if (table->buckets[bucket].tree != NULL && table->buckets[bucket].hash == hash && necro_decision_tree_is_equal(tree, table->buckets[bucket].tree))
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

NecroDecisionTree* necro_decision_tree_hash_table_maximal_sharing_go(NecroDecisionTreeHashTable* table, NecroDecisionTree* tree)
{
    assert(table != NULL);
    assert(tree != NULL);
    switch (tree->type)
    {
    case NECRO_DECISION_TREE_LEAF:
        return necro_decision_tree_hash_table_insert(table, tree);
    case NECRO_DECISION_TREE_SWITCH:
        for (size_t i = 0; i < tree->tree_switch.num_cases; ++i)
        {
            tree->tree_switch.cases[i] = necro_decision_tree_hash_table_maximal_sharing_go(table, tree->tree_switch.cases[i]);
        }
        return necro_decision_tree_hash_table_insert(table, tree);
    case NECRO_DECISION_TREE_LIT_SWITCH:
        for (size_t i = 0; i < tree->tree_lit_switch.num_cases; ++i)
        {
            tree->tree_lit_switch.cases[i] = necro_decision_tree_hash_table_maximal_sharing_go(table, tree->tree_lit_switch.cases[i]);
        }
        return necro_decision_tree_hash_table_insert(table, tree);
    default:
        assert(false && "Unrecognized tree type in necro_maximal_sharing_go");
        return NULL;
    }
}

NecroDecisionTree* necro_decision_tree_maximal_sharing(NecroDecisionTree* tree)
{
    size_t                     size  = (size_t)next_highest_pow_of_2((uint32_t)necro_decision_tree_count_nodes(tree));
    NecroDecisionTreeHashTable table = necro_decision_tree_hash_table_create(size);
    necro_decision_tree_hash_table_maximal_sharing_go(&table, tree);
    necro_decision_tree_hash_table_destroy(&table);
    return tree;
}

void necro_decision_tree_print_go(NecroMachProgram* program, NecroDecisionTree* tree, size_t depth)
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
        printf("tree: %p, slot: %zu\n", tree, tree->tree_switch.pattern->parent_slot);
        for (size_t i = 0; i < tree->tree_switch.num_cases; ++i)
        {

            print_white_space(depth + 2);
            printf("*%s:\n", tree->tree_switch.symbols[i]->name->str);
            necro_decision_tree_print_go(program, tree->tree_switch.cases[i], depth + 4);
        }
        break;
    }
    case NECRO_DECISION_TREE_LIT_SWITCH:
    {
        print_white_space(depth);
        printf("tree: %p, slot: %zu\n", tree, tree->tree_lit_switch.pattern->parent_slot);
        for (size_t i = 0; i < tree->tree_lit_switch.num_cases; ++i)
        {

            print_white_space(depth + 2);
            switch (tree->tree_lit_switch.constants[i].type)
            {
            case NECRO_AST_CONSTANT_INTEGER:
            case NECRO_AST_CONSTANT_INTEGER_PATTERN:
                printf("*%" PRId64 ":\n", tree->tree_lit_switch.constants[i].int_literal);
                break;
            case NECRO_AST_CONSTANT_FLOAT:
            case NECRO_AST_CONSTANT_FLOAT_PATTERN:
                printf("*%f:\n", tree->tree_lit_switch.constants[i].float_literal);
                break;
            case NECRO_AST_CONSTANT_CHAR:
            case NECRO_AST_CONSTANT_CHAR_PATTERN:
                printf("*%c:\n", tree->tree_lit_switch.constants[i].char_literal);
                break;
            case NECRO_AST_CONSTANT_STRING:
                printf("*_:\n");
                break;
            default:
                assert(false && "Unhandled constant type");
                break;
            }
            necro_decision_tree_print_go(program, tree->tree_lit_switch.cases[i], depth + 4);
        }
        break;
    }
    default:
        assert(false);
        break;
    }
}

void necro_decision_tree_print(NecroMachProgram* program, NecroDecisionTree* tree)
{
    necro_decision_tree_print_go(program, tree, 0);
}

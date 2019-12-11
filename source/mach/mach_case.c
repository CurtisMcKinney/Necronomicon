/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "mach_case.h"
#include "symtable.h"
#include "utility/math_utility.h"
#include "mach_type.h"
#include "mach_transform.h"

/*
    TODO:
        * More testing
        * Redundant patterns error in case statements!
        * Handle top better?
        * Find better/faster algorithm for block env?
        * Get rid of find check in env cache?
        * Verify register/block domain correctness?
*/

//=====================================================
// Forward declaration
//=====================================================
struct NecroDecisionTree;
struct NecroDecisionTree* necro_decision_tree_maximal_sharing(struct NecroDecisionTree* tree);
void                      necro_decision_tree_print(NecroMachProgram* program, struct NecroDecisionTree* tree);


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

NECRO_DECLARE_ARENA_LIST(struct NecroPattern*, Pattern, pattern);

typedef struct NecroPattern
{
    struct NecroPattern*      parent;
    size_t                    parent_slot;
    NecroMachType*            value_type;
    NecroMachAst*             value_ast;
    NecroCoreAst*             pat_ast;
    NECRO_PATTERN_TYPE        pattern_type;
} NecroPattern;

typedef enum
{
    NECRO_DECISION_TREE_LEAF,
    NECRO_DECISION_TREE_SWITCH,
    NECRO_DECISION_TREE_LIT_SWITCH,
    NECRO_DECISION_TREE_BIND,
} NECRO_DECISION_TREE_TYPE;

typedef struct
{
    NecroCoreAst* expression;
    NecroPattern* pattern;
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
    NecroPatternList*        bindings;
} NecroDecisionTree;

/*
    Revised NecroPatternMatrix layout, R x C:

    pat[0][0] pat[0][1] ... pat[0][C] | -> exp[0]
    pat[1][0] pat[1][1] ... pat[1][C] | -> exp[1]
    ...       ...       ... ...            ...
    pat[R][0] pat[R][1] ... pat[R][C] | -> exp[R]
*/

typedef struct NecroPatternMatrix
{
    NecroPattern***   patterns;
    NecroCoreAst**    expressions;
    NecroPatternList* bindings;
    size_t            rows;
    size_t            columns;
} NecroPatternMatrix;

NecroCoreAst* necro_core_ast_unwrap_apps(NecroCoreAst* ast)
{
    assert(ast != NULL);
    while (ast->ast_type == NECRO_CORE_AST_APP)
        ast = ast->app.expr1;
    assert(ast->ast_type == NECRO_CORE_AST_VAR);
    return ast;
}

NecroPattern* necro_pattern_alloc(NecroMachProgram* program, NecroPattern* parent, size_t parent_slot, NecroMachAst* value_ast, NecroMachType* value_type, NecroCoreAst* pat_ast, NECRO_PATTERN_TYPE pattern_type)
{
    assert(pat_ast != NULL);
    if (value_ast != NULL && value_type != NULL)
        necro_mach_type_check(program, value_ast->necro_machine_type, value_type);
    NecroPattern* pat = necro_paged_arena_alloc(&program->arena, sizeof(NecroPattern));
    pat->parent       = parent;
    pat->parent_slot  = parent_slot;
    pat->value_type   = value_type;
    pat->value_ast    = value_ast;
    pat->pat_ast      = pat_ast;
    pat->pattern_type = pattern_type;
    return pat;
}

NecroPattern* necro_pattern_create_top(NecroMachProgram* program, NecroCoreAst* pat_ast, NecroMachAst* outer)
{
    NecroMachAst*  value      = necro_core_transform_to_mach_3_go(program, pat_ast, outer);
    NecroMachType* value_type = necro_mach_type_make_ptr_if_boxed(program, necro_mach_type_from_necro_type(program, pat_ast->necro_type));
    return necro_pattern_alloc(program, NULL, 0, value, value_type, pat_ast, NECRO_PATTERN_TOP);
}

NecroPattern* necro_pattern_create_wildcard(NecroMachProgram* program, NecroPattern* parent, NecroCoreAst* var_ast)
{
    return necro_pattern_alloc(program, parent, 0, NULL, NULL, var_ast, NECRO_PATTERN_WILDCARD);
}

NecroPattern* necro_pattern_copy(NecroMachProgram* program, NecroPattern* pattern)
{
    return necro_pattern_alloc(program, pattern->parent, pattern->parent_slot, pattern->value_ast, pattern->value_type, pattern->pat_ast, pattern->pattern_type);
}

NecroPattern* necro_pattern_create(NecroMachProgram* program, NecroPattern* parent, NecroCoreAst* pat_ast, const size_t slot)
{
    assert(pat_ast != NULL);
    switch (pat_ast->ast_type)
    {
    case NECRO_CORE_AST_VAR:
    {
        if (pat_ast->var.ast_symbol->is_wildcard)
        {
            return necro_pattern_create_wildcard(program, parent, pat_ast);
        }
        pat_ast->var.ast_symbol->mach_symbol = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, pat_ast->var.ast_symbol);
        NecroMachAstSymbol* symbol = pat_ast->var.ast_symbol->mach_symbol;
        if (symbol->is_enum)
        {
            NecroMachAst* enum_value = necro_mach_value_create_word_uint(program, symbol->con_num);
            return necro_pattern_alloc(program, parent, slot, enum_value, program->type_cache.word_uint_type, pat_ast, NECRO_PATTERN_ENUM);
        }
        else if (symbol->is_constructor)
        {
            NecroMachType* con_type = necro_mach_type_create_ptr(&program->arena, necro_mach_type_from_necro_type(program, pat_ast->necro_type));
            return necro_pattern_alloc(program, parent, slot, NULL, con_type, pat_ast, NECRO_PATTERN_CON);
        }
        else
        {
            if (symbol->mach_type == NULL)
                symbol->mach_type = necro_mach_type_make_ptr_if_boxed(program, necro_mach_type_from_necro_type(program, pat_ast->necro_type));
            NecroPattern*  pat = necro_pattern_alloc(program, parent, slot, NULL, symbol->mach_type, pat_ast, NECRO_PATTERN_VAR);
            return pat;
        }
    }
    case NECRO_CORE_AST_APP:
    {
        NecroCoreAst*       unwrapped  = necro_core_ast_unwrap_apps(pat_ast);
        NecroCoreAstSymbol* ast_symbol = unwrapped->var.ast_symbol;
        assert(ast_symbol->mach_symbol != NULL);
        NecroMachType*      con_type   = ast_symbol->mach_symbol->mach_type->fn_type.parameters[0];
        NecroPattern*       pat        = necro_pattern_alloc(program, parent, slot, NULL, con_type, pat_ast, NECRO_PATTERN_APP);
        return pat;
    }
    case NECRO_CORE_AST_LIT:
    {
        NecroMachAst* lit_value = necro_core_transform_to_mach_3_go(program, pat_ast, NULL);
        return necro_pattern_alloc(program, parent, slot, lit_value, lit_value->necro_machine_type, pat_ast, NECRO_PATTERN_LIT);
    }
    default:
        assert(false);
        return NULL;
    }
}

NecroDecisionTree* necro_decision_tree_create_leaf(NecroMachProgram* program, NecroPattern* pattern, NecroCoreAst* leaf_expression, NecroPatternList* bindings)
{
    NecroDecisionTree* leaf    = necro_paged_arena_alloc(&program->arena, sizeof(NecroDecisionTree));
    leaf->type                 = NECRO_DECISION_TREE_LEAF;
    leaf->tree_leaf.pattern    = pattern;
    leaf->tree_leaf.expression = leaf_expression;
    leaf->block                = NULL;
    leaf->hash                 = 0;
    leaf->bindings             = bindings;
    return leaf;
}

NecroDecisionTree* necro_decision_tree_create_switch(NecroMachProgram* program, NecroPattern* pattern, NecroDecisionTree** cases, NecroCoreAstSymbol** symbols, size_t num_cases, NecroPatternList* bindings)
{
    NecroDecisionTree* tree_switch     = necro_paged_arena_alloc(&program->arena, sizeof(NecroDecisionTree));
    tree_switch->type                  = NECRO_DECISION_TREE_SWITCH;
    tree_switch->tree_switch.pattern   = pattern;
    tree_switch->tree_switch.cases     = cases;
    tree_switch->tree_switch.symbols   = symbols;
    tree_switch->tree_switch.num_cases = num_cases;
    tree_switch->block                 = NULL;
    tree_switch->hash                  = 0;
    tree_switch->bindings              = bindings;
    return tree_switch;
}

NecroDecisionTree* necro_decision_tree_create_lit_switch(NecroMachProgram* program, NecroPattern* pattern, NecroDecisionTree** cases, NecroCoreAstLiteral* constants, size_t num_cases, NecroPatternList* bindings)
{
    NecroDecisionTree* tree_switch         = necro_paged_arena_alloc(&program->arena, sizeof(NecroDecisionTree));
    tree_switch->type                      = NECRO_DECISION_TREE_LIT_SWITCH;
    tree_switch->tree_lit_switch.pattern   = pattern;
    tree_switch->tree_lit_switch.cases     = cases;
    tree_switch->tree_lit_switch.constants = constants;
    tree_switch->tree_lit_switch.num_cases = num_cases;
    tree_switch->block                     = NULL;
    tree_switch->hash                      = 0;
    tree_switch->bindings                  = bindings;
    return tree_switch;
}

bool necro_decision_tree_is_branchless(NecroDecisionTree* tree)
{
switch (tree->type)
{
case NECRO_DECISION_TREE_LEAF: return true;
case NECRO_DECISION_TREE_SWITCH:
{
    assert(tree->tree_switch.num_cases > 0);
    if (tree->tree_switch.num_cases > 1)
        return false;
    else
        return necro_decision_tree_is_branchless(tree->tree_switch.cases[0]);
}
case NECRO_DECISION_TREE_LIT_SWITCH:
{
    assert(tree->tree_switch.num_cases > 0);
    if (tree->tree_lit_switch.num_cases > 1)
        return false;
    else
        return necro_decision_tree_is_branchless(tree->tree_lit_switch.cases[0]);
}
default:
    assert(false);
    return false;
}
}

NecroDecisionTree* necro_pattern_matrix_compile(NecroMachProgram* program, NecroPatternMatrix* matrix, NecroCoreAst* top_case_ast);

NecroPatternMatrix necro_pattern_matrix_create(NecroMachProgram* program, size_t rows, size_t columns)
{
    NecroPattern*** patterns = necro_paged_arena_alloc(&program->arena, rows * sizeof(NecroPattern*));
    NecroCoreAst** expressions = necro_paged_arena_alloc(&program->arena, rows * sizeof(NecroCoreAst*));
    for (size_t r = 0; r < rows; ++r)
    {
        patterns[r] = necro_paged_arena_alloc(&program->arena, columns * sizeof(NecroCoreAst*));
        for (size_t c = 0; c < columns; ++c)
        {
            patterns[r][c] = NULL;
        }
        expressions[r] = NULL;
    }
    return (NecroPatternMatrix)
    {
        .rows = rows,
            .columns = columns,
            .patterns = patterns,
            .expressions = expressions,
            .bindings = NULL,
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
    NecroPatternMatrix matrix = necro_pattern_matrix_create(program, rows, columns);
    size_t             this_rows = 0;
    alts = ast->case_expr.alts;
    while (alts != NULL)
    {
        NecroCoreAst* alt = alts->data;
        assert(alt->case_alt.pat != NULL);
        matrix.patterns[this_rows][0] = necro_pattern_create(program, case_expr_pattern, alt->case_alt.pat, 0);
        if (matrix.patterns[this_rows][0]->pattern_type == NECRO_PATTERN_VAR)
            matrix.bindings = necro_cons_pattern_list(&program->arena, matrix.patterns[this_rows][0], matrix.bindings);
        matrix.expressions[this_rows] = alt->case_alt.expr;
        this_rows++;
        alts = alts->next;
    }
    return matrix;
}

NecroPatternMatrix necro_pattern_matrix_specialize(NecroMachProgram* program, NecroPatternMatrix* matrix, NecroCoreAstSymbol* pattern_symbol, NecroType* data_con_necro_type)
{
    size_t con_arity = necro_type_arity(data_con_necro_type);
    size_t new_rows = 0;
    size_t new_columns = con_arity + (matrix->columns - 1);
    new_columns = (new_columns == SIZE_MAX) ? 0 : new_columns;
    for (size_t r = 0; r < matrix->rows; ++r)
    {
        NecroPattern* row_head = matrix->patterns[r][0];
        assert(row_head != NULL);
        if (row_head->pattern_type == NECRO_PATTERN_WILDCARD || row_head->pattern_type == NECRO_PATTERN_VAR)
        {
            new_rows++;
        }
        else if (row_head->pattern_type == NECRO_PATTERN_CON || row_head->pattern_type == NECRO_PATTERN_APP)
        {
            NecroCoreAst*       unwrapped  = necro_core_ast_unwrap_apps(row_head->pat_ast);
            NecroCoreAstSymbol* ast_symbol = unwrapped->var.ast_symbol;
            if (ast_symbol == pattern_symbol)
                new_rows++;
        }
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
            for (size_t c_d = 0; c_d < column_diff; ++c_d)
            {
                specialized_matrix.patterns[new_r][c_d] = row_head;
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
            NecroCoreAst*       unwrapped  = necro_core_ast_unwrap_apps(row_head->pat_ast);
            NecroCoreAstSymbol* ast_symbol = unwrapped->var.ast_symbol;
            assert(ast_symbol->is_constructor);
            if (ast_symbol != pattern_symbol) // Delete other constructors
                continue;
            NecroCoreAst* apps = row_head->pat_ast;
            for (size_t c_d_rev = 0; c_d_rev < column_diff; ++c_d_rev)
            {
                assert(apps->ast_type == NECRO_CORE_AST_APP);
                const size_t  c_d                       = (column_diff - 1) - c_d_rev;
                NecroCoreAst* pat                       = apps->app.expr2;
                specialized_matrix.patterns[new_r][c_d] = necro_pattern_create(program, row_head, pat, c_d);
                if (specialized_matrix.patterns[new_r][c_d]->pattern_type == NECRO_PATTERN_VAR)
                    specialized_matrix.bindings = necro_cons_pattern_list(&program->arena, specialized_matrix.patterns[new_r][c_d], specialized_matrix.bindings);
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

bool necro_core_ast_lit_is_equal(NecroCoreAstLiteral lit1, NecroCoreAstLiteral lit2)
{
    if (lit1.type != lit2.type)
        return false;
    switch (lit1.type)
    {
    case NECRO_AST_CONSTANT_FLOAT:
    case NECRO_AST_CONSTANT_FLOAT_PATTERN:
        return lit1.float_literal == lit2.float_literal; // Floats not being handled very well here...
    case NECRO_AST_CONSTANT_INTEGER:
    case NECRO_AST_CONSTANT_INTEGER_PATTERN:
        return lit1.int_literal == lit2.int_literal;
    case NECRO_AST_CONSTANT_CHAR:
    case NECRO_AST_CONSTANT_CHAR_PATTERN:
        return lit1.char_literal == lit2.char_literal;
    default:
        assert(false && "Found Non-Pattern literal while compiling case statement");
        return false;
    }
}

NecroPatternMatrix necro_pattern_matrix_specialize_lit(NecroMachProgram* program, NecroPatternMatrix* matrix, NecroCoreAstLiteral lit)
{
    size_t new_rows    = 0;
    size_t new_columns = (matrix->columns == 0) ? 0 : matrix->columns - 1;
    for (size_t r = 0; r < matrix->rows; ++r)
    {
        NecroPattern* row_head = matrix->patterns[r][0];
        assert(row_head != NULL);
        if (row_head->pattern_type == NECRO_PATTERN_WILDCARD || row_head->pattern_type == NECRO_PATTERN_VAR)
        {
            new_rows++;
        }
        else if (row_head->pattern_type == NECRO_PATTERN_ENUM)
        {
            if (lit.type == NECRO_AST_CONSTANT_INTEGER && row_head->pat_ast->var.ast_symbol->con_num == lit.uint_literal)
                new_rows++;
        }
        else if (row_head->pattern_type == NECRO_PATTERN_LIT)
        {
            if (necro_core_ast_lit_is_equal(row_head->pat_ast->lit, lit))
                new_rows++;
        }
        else
        {
            assert(false);
        }
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
            for (size_t c_d = 0; c_d < column_diff; ++c_d)
            {
                specialized_matrix.patterns[new_r][c_d] = row_head;
            }
            for (size_t c = 0; c < matrix->columns - 1; ++c)
            {
                specialized_matrix.patterns[new_r][column_diff + c] = matrix->patterns[r][c + 1];
            }
            specialized_matrix.expressions[new_r] = matrix->expressions[r];
        }
        else if (lit.type == NECRO_AST_CONSTANT_STRING)
        {
            // HACK: Why the fuck are we using string to indicate that the pattern is a wildcard / variable? Switch lit type to NecroPattern to get rid of this bullshit
            continue;
        }
        else if (row_head->pattern_type == NECRO_PATTERN_LIT || row_head->pattern_type == NECRO_PATTERN_ENUM) // Lit
        {
            bool is_specialized_case = (row_head->pattern_type == NECRO_PATTERN_LIT) ? necro_core_ast_lit_is_equal(row_head->pat_ast->lit, lit) : row_head->pat_ast->var.ast_symbol->con_num == lit.uint_literal;
            if (!is_specialized_case)
                continue;
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

NecroPatternMatrix* necro_pattern_matrix_drop_columns(NecroPatternMatrix* matrix, size_t num_drop)
{
    for (size_t r = 0; r < matrix->rows; ++r)
    {
        matrix->patterns[r] += num_drop;
    }
    matrix->columns -= num_drop;
    return matrix;
}

NecroDecisionTree* necro_pattern_matrix_finish_compile_literal(NecroMachProgram* program, NecroPattern* switch_pattern, NecroPatternMatrix* matrix, NecroCoreAst* top_case_ast, size_t num_drop, NecroPatternList* bindings)
{
    size_t num_cases = 0;
    for (size_t i = 0; i < matrix->rows; ++i)
    {
        num_cases++;
        if (matrix->patterns[i][0]->pattern_type == NECRO_PATTERN_WILDCARD || matrix->patterns[i][0]->pattern_type == NECRO_PATTERN_VAR)
            break;
    }
    matrix                         = necro_pattern_matrix_drop_columns(matrix, num_drop);
    NecroDecisionTree**  cases     = necro_paged_arena_alloc(&program->arena, num_cases * sizeof(NecroDecisionTree*));
    NecroCoreAstLiteral* constants = necro_paged_arena_alloc(&program->arena, num_cases * sizeof(NecroCoreAstLiteral));
    for (size_t r = 0; r < matrix->rows; ++r)
    {
        if (matrix->patterns[r][0]->pattern_type == NECRO_PATTERN_WILDCARD || matrix->patterns[r][0]->pattern_type == NECRO_PATTERN_VAR)
        {
            NecroCoreAstLiteral lit        = (NecroCoreAstLiteral) { .type = NECRO_AST_CONSTANT_STRING, .string_literal = NULL };
            NecroPatternMatrix  con_matrix = necro_pattern_matrix_specialize_lit(program, matrix, lit);
            cases[r]                       = necro_pattern_matrix_compile(program, &con_matrix, top_case_ast);
            constants[r]                   = lit;
            break;
        }
        else if (matrix->patterns[r][0]->pattern_type == NECRO_PATTERN_ENUM)
        {
            NecroCoreAstLiteral lit       = (NecroCoreAstLiteral) { .type = NECRO_AST_CONSTANT_INTEGER, .uint_literal = matrix->patterns[r][0]->pat_ast->var.ast_symbol->con_num };
            NecroPatternMatrix con_matrix = necro_pattern_matrix_specialize_lit(program, matrix, lit);
            cases[r]                      = necro_pattern_matrix_compile(program, &con_matrix, top_case_ast);
            constants[r]                  = lit;
        }
        else if (matrix->patterns[r][0]->pattern_type == NECRO_PATTERN_LIT)
        {
            NecroPatternMatrix con_matrix = necro_pattern_matrix_specialize_lit(program, matrix, matrix->patterns[r][0]->pat_ast->lit);
            cases[r]                      = necro_pattern_matrix_compile(program, &con_matrix, top_case_ast);
            constants[r]                  = matrix->patterns[r][0]->pat_ast->lit;
        }
        else
        {
            assert(false);
        }
    }
    return necro_decision_tree_create_lit_switch(program, switch_pattern, cases, constants, num_cases, bindings);
}

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

NecroDecisionTree* necro_pattern_matrix_finish_compile(NecroMachProgram* program, NecroPattern* switch_pattern, NecroPatternMatrix* matrix, NecroCoreAst* top_case_ast, size_t num_drop, NecroType* pattern_type, NecroPatternList* bindings)
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
    return necro_decision_tree_create_switch(program, switch_pattern, cases, symbols, num_cons, bindings);
}

NecroDecisionTree* necro_pattern_matrix_compile(NecroMachProgram* program, NecroPatternMatrix* matrix, NecroCoreAst* top_case_ast)
{
    if (matrix->rows == 0)
    {
        assert(false && "Non-exhaustive patterns in case statement!");
        return NULL;
    }
    bool all_wildcards = true;
    for (size_t c = 0; c < matrix->columns && all_wildcards; ++c)
    {
        all_wildcards = matrix->patterns[0][c]->pattern_type == NECRO_PATTERN_WILDCARD || matrix->patterns[0][c]->pattern_type == NECRO_PATTERN_VAR;
    }
    if (all_wildcards)
    {
        if (matrix->columns > 0)
            return necro_decision_tree_create_leaf(program, matrix->patterns[0][0]->parent, matrix->expressions[0], matrix->bindings);
        else
            return necro_decision_tree_create_leaf(program, NULL, matrix->expressions[0], matrix->bindings);
    }
    // For now, doing simple Left-to-Right heuristic
    // Find pattern type and num_cons and drop all empty columns
    NecroType* pattern_type = NULL;
    size_t     num_drop     = 0;
    for (size_t c = 0; c < matrix->columns; ++c)
    {
        for (size_t r = 0; r < matrix->rows; ++r)
        {
            if (matrix->patterns[r][c]->pattern_type == NECRO_PATTERN_WILDCARD || matrix->patterns[r][c]->pattern_type == NECRO_PATTERN_VAR)
            {
            }
            else if (matrix->patterns[r][c]->pattern_type == NECRO_PATTERN_CON || matrix->patterns[r][c]->pattern_type == NECRO_PATTERN_APP)
            {
                NecroType*    necro_type     = matrix->patterns[r][c]->pat_ast->necro_type;
                NecroPattern* switch_pattern = matrix->patterns[r][c];
                if (necro_mach_type_is_sum_type(switch_pattern->value_type))
                {
                    switch_pattern             = necro_pattern_copy(program, switch_pattern);
                    switch_pattern->value_type = necro_mach_type_make_ptr_if_boxed(program, necro_mach_type_get_sum_type(switch_pattern->value_type));
                }
                return necro_pattern_matrix_finish_compile(program, switch_pattern, matrix, top_case_ast, num_drop, necro_type, matrix->bindings);
            }
            else if (matrix->patterns[r][c]->pattern_type == NECRO_PATTERN_LIT || matrix->patterns[r][c]->pattern_type == NECRO_PATTERN_ENUM)
            {
                // NecroPattern* switch_pattern = matrix->patterns[r][c];
                NecroSymbol         switch_value_name   = necro_intern_unique_string(program->intern, "lit");
                NecroCoreAstSymbol* switch_value_symbol = necro_core_ast_symbol_create(&program->arena, switch_value_name, matrix->patterns[r][c]->pat_ast->necro_type);
                NecroCoreAst*       switch_value_var    = necro_core_ast_create_var(&program->arena, switch_value_symbol, matrix->patterns[r][c]->pat_ast->necro_type);
                NecroPattern*       switch_pattern      = necro_pattern_create(program, matrix->patterns[r][c]->parent, switch_value_var, matrix->patterns[r][c]->parent_slot);
                return necro_pattern_matrix_finish_compile_literal(program, switch_pattern, matrix, top_case_ast, num_drop, matrix->bindings);
            }
            else
            {
                assert(false);
            }
        }
        num_drop++;
    }
    return necro_pattern_matrix_finish_compile(program, NULL, matrix, top_case_ast, num_drop, pattern_type, matrix->bindings);
}


///////////////////////////////////////////////////////
// NecroBlockEnv
///////////////////////////////////////////////////////
typedef struct NecroLoadCache
{
    NecroMachAst*  parent_value;
    size_t         slot;
    NecroMachType* loaded_value_type;
    NecroMachAst*  loaded_value;
} NecroLoadCache;

NECRO_DECLARE_ARENA_LIST(NecroLoadCache, LoadCache, load_cache);

typedef struct NecroBlockEnv
{
    struct NecroBlockEnv* parent;
    NecroMachAst*         block;
    NecroLoadCacheList*   cached_values;
} NecroBlockEnv;

NecroBlockEnv* necro_block_env_create(NecroPagedArena* arena, NecroMachAst* block, NecroBlockEnv* parent)
{
    assert(block->type == NECRO_MACH_BLOCK);
    NecroBlockEnv* env = necro_paged_arena_alloc(arena, sizeof(NecroBlockEnv));
    env->parent        = parent;
    env->block         = block;
    env->cached_values = NULL;
    return env;
}

NecroMachAst* necro_block_env_find(NecroBlockEnv* block_env, NecroMachAst* parent_value, size_t slot, NecroMachType* loaded_value_type)
{
    assert(block_env != NULL);
    while (block_env != NULL)
    {
        NecroLoadCacheList* cached_values = block_env->cached_values;
        while (cached_values != NULL)
        {
            if (cached_values->data.parent_value == parent_value && cached_values->data.slot == slot && necro_mach_type_is_eq(cached_values->data.loaded_value_type, loaded_value_type))
            {
                assert(cached_values->data.loaded_value != NULL);
                return cached_values->data.loaded_value;
            }
            cached_values = cached_values->next;
        }
        block_env = block_env->parent;
    }
    return NULL;
}

// Note, top's parent_value == NULL
void necro_block_env_cache_value(NecroPagedArena* arena, NecroBlockEnv* block_env, NecroMachAst* parent_value, size_t slot, NecroMachType* loaded_value_type, NecroMachAst* loaded_value)
{
    NecroMachAst*  cached_value = necro_block_env_find(block_env, parent_value, slot, loaded_value_type);
    if (cached_value != NULL)
        return;
    NecroLoadCache cache     = (NecroLoadCache) { .parent_value = parent_value, .slot = slot, .loaded_value_type = loaded_value_type, .loaded_value = loaded_value };
    block_env->cached_values = necro_cons_load_cache_list(arena, cache, block_env->cached_values);
}


///////////////////////////////////////////////////////
// Decision Tree to Mach
///////////////////////////////////////////////////////
void necro_decision_tree_pattern_to_mach(NecroMachProgram* program, NecroPattern* pattern, NecroMachAst* outer, NecroPatternList* bindings, NecroBlockEnv* env);

void necro_pattern_var_to_mach(NecroMachProgram* program, NecroPattern* pattern, NecroMachAst* outer, NecroBlockEnv* env)
{
    assert(pattern != NULL);
    assert(pattern->pattern_type == NECRO_PATTERN_VAR);

    //--------------------
    // Resolve Parent value
    if (pattern->parent->value_ast == NULL)
        necro_decision_tree_pattern_to_mach(program, pattern->parent, outer, NULL, env);

    // //--------------------
    // // Find cached pattern
    // NecroMachAst*  cache_p_val  = pattern->parent->value_ast;
    // size_t         cache_p_slot = pattern->parent_slot;
    // NecroMachType* cache_type   = pattern->value_type;
    // NecroMachAst*  cached_value = necro_block_env_find(env, cache_p_val, cache_p_slot, cache_type);
    // if (cached_value != NULL)
    // {
    //     if (pattern->pat_ast->var.ast_symbol->mach_symbol == NULL)
    //         pattern->pat_ast->var.ast_symbol->mach_symbol = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, pattern->pat_ast->var.ast_symbol);
    //     if (pattern->pat_ast->var.ast_symbol->mach_symbol->ast == NULL)
    //     {
    //         pattern->pat_ast->var.ast_symbol->mach_symbol->ast       = cached_value;
    //         pattern->pat_ast->var.ast_symbol->mach_symbol->mach_type = pattern->pat_ast->var.ast_symbol->mach_symbol->ast->necro_machine_type;
    //         pattern->value_ast                                       = pattern->pat_ast->var.ast_symbol->mach_symbol->ast;
    //         pattern->value_type                                      = pattern->pat_ast->var.ast_symbol->mach_symbol->mach_type;
    //     }
    //     return;
    // }
    // assert(pattern->value_ast == NULL);

    // Redo without env caching and just use pattern->pat_ast->var.ast_symbol->mach_symbol && pattern->pat_ast->var.ast_symbol->mach_symbol->ast to check for previously created pattern variables?

    //--------------------
    // Load and cache pattern
    NecroCoreAst* var_ast = pattern->pat_ast;
    NecroPattern* parent  = pattern->parent;
    // assert(pattern->pat_ast->var.ast_symbol->mach_symbol->ast == NULL);
    assert(pattern->parent != NULL);
    assert(pattern->parent->value_ast != NULL);

    if (var_ast->var.ast_symbol->mach_symbol == NULL)
        var_ast->var.ast_symbol->mach_symbol = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, var_ast->var.ast_symbol);

    if (var_ast->var.ast_symbol->mach_symbol->ast != NULL)
        return;

    NecroMachAst* parent_value = pattern->parent->value_ast;
    if (parent->pattern_type == NECRO_PATTERN_TOP)
    {
        var_ast->var.ast_symbol->mach_symbol->ast = parent_value;
    }
    // else if (necro_mach_type_is_unboxed(program, parent->value_type))
    else if (necro_mach_type_is_unboxed(program, parent_value->necro_machine_type))
    {
        var_ast->var.ast_symbol->mach_symbol->ast = necro_mach_build_extract_value(program, outer->machine_def.update_fn, parent_value, pattern->parent_slot, "value");
    }
    else
    {
        NecroMachAst* slot_ptr                    = necro_mach_build_gep(program, outer->machine_def.update_fn, parent_value, (uint32_t[]) { 0, pattern->parent_slot + 1 }, 2, "value_ptr");
        var_ast->var.ast_symbol->mach_symbol->ast = necro_mach_build_load(program, outer->machine_def.update_fn, slot_ptr, "value");
    }
    var_ast->var.ast_symbol->mach_symbol->mach_type = var_ast->var.ast_symbol->mach_symbol->ast->necro_machine_type;
    pattern->value_ast                              = pattern->pat_ast->var.ast_symbol->mach_symbol->ast;
    pattern->value_type                             = pattern->pat_ast->var.ast_symbol->mach_symbol->mach_type;
    necro_block_env_cache_value(&program->arena, env, pattern->parent->value_ast, pattern->parent_slot, pattern->value_type, pattern->value_ast);
}

void necro_decision_tree_pattern_to_mach(NecroMachProgram* program, NecroPattern* pattern, NecroMachAst* outer, NecroPatternList* bindings, NecroBlockEnv* env)
{
    if (pattern == NULL)
        return;

    //--------------------
    // Wildcard
    if (pattern->pattern_type == NECRO_PATTERN_WILDCARD)
    {
        assert(bindings == NULL);
        return;
    }

    //--------------------
    // Top
    if (pattern->pattern_type == NECRO_PATTERN_TOP || pattern->pattern_type == NECRO_PATTERN_LIT || pattern->pattern_type == NECRO_PATTERN_ENUM)
    {
        // assert(bindings == NULL);
        assert(pattern->value_ast != NULL);
        assert(pattern->value_type != NULL);
        necro_block_env_cache_value(&program->arena, env, NULL, 0, pattern->value_type, pattern->value_ast);
        // Transform bindings
        while (bindings != NULL)
        {
            necro_pattern_var_to_mach(program, bindings->data, outer, env);
            bindings = bindings->next;
        }
        return;
    }

    //--------------------
    // Resolve Parent value
    if (pattern->parent->value_ast == NULL)
        necro_decision_tree_pattern_to_mach(program, pattern->parent, outer, NULL, env);

    //--------------------
    // VAR
    if (pattern->pattern_type == NECRO_PATTERN_VAR)
    {
        necro_pattern_var_to_mach(program, pattern, outer, env);
        // Transform bindings
        while (bindings != NULL)
        {
            necro_pattern_var_to_mach(program, bindings->data, outer, env);
            bindings = bindings->next;
        }
        return;
    }

    //--------------------
    // Load / Downcast / Cache
    NecroMachAst* cached_value = necro_block_env_find(env, pattern->parent->value_ast, pattern->parent_slot, pattern->value_type);
    if (cached_value == NULL)
    {
        NecroPattern* parent = pattern->parent;
        assert(parent != NULL);
        assert(parent->value_ast != NULL);
        // // Unboxed Value Types
        // if (pattern->value_type->type == NECRO_MACH_TYPE_STRUCT && pattern->value_type->struct_type.symbol->is_unboxed)
        // {
        //     pattern->value_ast = parent->value_ast;
        // }
        // Sum type
        if (necro_mach_type_is_sum_type(pattern->value_type))
        {
            assert(pattern->value_ast == NULL);
            NecroMachType* sum_type = necro_mach_type_make_ptr_if_boxed(program, necro_mach_type_get_sum_type(pattern->value_type));
            cached_value            = necro_block_env_find(env, pattern->parent->value_ast, pattern->parent_slot, sum_type);
            if (cached_value == NULL)
            {
                if (parent->pattern_type == NECRO_PATTERN_TOP)
                {
                    cached_value       = parent->value_ast;
                    pattern->value_ast = necro_mach_build_down_cast(program, outer->machine_def.update_fn, cached_value, pattern->value_type);
                }
                // else if (necro_mach_type_is_unboxed(program, parent->value_type))
                else if (necro_mach_type_is_unboxed(program, parent->value_ast->necro_machine_type))
                {
                    pattern->value_ast = necro_mach_build_extract_value(program, outer->machine_def.update_fn, parent->value_ast, pattern->parent_slot, "value");
                    pattern->value_ast = necro_mach_build_down_cast(program, outer->machine_def.update_fn, cached_value, pattern->value_type);
                }
                else
                {
                    NecroMachAst* slot_ptr = necro_mach_build_gep(program, outer->machine_def.update_fn, parent->value_ast, (uint32_t[]) { 0, pattern->parent_slot + 1 }, 2, "slot_ptr");
                    cached_value           = necro_mach_build_load(program, outer->machine_def.update_fn, slot_ptr, "value");
                    pattern->value_ast     = necro_mach_build_down_cast(program, outer->machine_def.update_fn, cached_value, pattern->value_type);
                }
                necro_block_env_cache_value(&program->arena, env, pattern->parent->value_ast, pattern->parent_slot, cached_value->necro_machine_type, cached_value); // Cache sum type
                necro_block_env_cache_value(&program->arena, env, pattern->parent->value_ast, pattern->parent_slot, pattern->value_type, pattern->value_ast); // Cache cast type
            }
            else
            {
                pattern->value_ast  = necro_mach_build_down_cast(program, outer->machine_def.update_fn, cached_value, pattern->value_type);
                necro_block_env_cache_value(&program->arena, env, pattern->parent->value_ast, pattern->parent_slot, pattern->value_type, pattern->value_ast);
            }
        }
        // Non-Sum Type
        else
        {
            if (parent->pattern_type == NECRO_PATTERN_TOP)
            {
                pattern->value_ast = parent->value_ast;
            }
            // else if (necro_mach_type_is_unboxed(program, parent->value_type))
            else if (necro_mach_type_is_unboxed(program, parent->value_ast->necro_machine_type))
            {
                assert(pattern->value_ast == NULL);
                pattern->value_ast = necro_mach_build_extract_value(program, outer->machine_def.update_fn, parent->value_ast, pattern->parent_slot, "value");
                necro_block_env_cache_value(&program->arena, env, pattern->parent->value_ast, pattern->parent_slot, pattern->value_type, pattern->value_ast);
            }
            else
            {
                assert(pattern->value_ast == NULL);
                NecroMachAst* slot_ptr = necro_mach_build_gep(program, outer->machine_def.update_fn, parent->value_ast, (uint32_t[]) { 0, pattern->parent_slot + 1 }, 2, "slot_ptr");
                pattern->value_ast     = necro_mach_build_load(program, outer->machine_def.update_fn, slot_ptr, "value");
                necro_block_env_cache_value(&program->arena, env, pattern->parent->value_ast, pattern->parent_slot, pattern->value_type, pattern->value_ast);
            }
        }
    }
    else
    {
        pattern->value_ast  = cached_value;
        pattern->value_type = cached_value->necro_machine_type;
    }

    //--------------------
    // Transform bindings
    while (bindings != NULL)
    {
        necro_pattern_var_to_mach(program, bindings->data, outer, env);
        bindings = bindings->next;
    }

}

NecroMachAst* necro_decision_tree_to_mach(NecroMachProgram* program, NecroDecisionTree* tree, NecroMachAst* term_case_block, NecroMachAst* error_block, NecroMachAst* result_phi, NecroMachAst* outer, NecroBlockEnv* env)
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
        necro_decision_tree_pattern_to_mach(program, tree->tree_leaf.pattern, outer, tree->bindings, env);
        NecroMachAst* leaf_value = necro_core_transform_to_mach_3_go(program, tree->tree_leaf.expression, outer);
        if (result_phi != NULL)
        {
            // Multiple cases
            NecroMachAst* curr_block = necro_mach_block_get_current(outer->machine_def.update_fn);
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
        necro_decision_tree_pattern_to_mach(program, tree->tree_switch.pattern, outer, tree->bindings, env);
        assert(tree->tree_switch.pattern->value_ast != NULL);
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
                    const char*    case_name  = necro_snapshot_arena_concat_strings(&program->snapshot_arena, 2, (const char*[]) { tree->tree_switch.symbols[i]->name->str, "_case" });
                    NecroMachAst*  case_block = necro_mach_block_insert_before(program, outer->machine_def.update_fn, case_name, term_case_block);
                    NecroBlockEnv* new_env    = necro_block_env_create(&program->arena, case_block, env);
                    necro_mach_add_case_to_switch(program, switch_value, case_block, i);
                    necro_mach_block_move_to(program, outer->machine_def.update_fn, case_block);
                    necro_decision_tree_to_mach(program, tree->tree_switch.cases[i], term_case_block, error_block, result_phi, outer, new_env);
                    tree->tree_switch.cases[i]->block = case_block;
                }
            }
            return NULL;
        }
        else
        {
            return necro_decision_tree_to_mach(program, tree->tree_switch.cases[0], term_case_block, error_block, result_phi, outer, env);
        }
    }
    case NECRO_DECISION_TREE_LIT_SWITCH:
    {
        necro_decision_tree_pattern_to_mach(program, tree->tree_lit_switch.pattern, outer, tree->bindings, env);
        assert(tree->tree_lit_switch.pattern->value_ast != NULL);
        if (tree->tree_lit_switch.num_cases > 1)
        {
            /*
                NOTE / HACK:
                    Bit casting uint, int, and float into uint to do a straight up bit comparison.
                    This is fine for uint and int, doing this with float can lead to "fun".
            */
            NecroMachAst*              value        = necro_mach_build_maybe_bit_cast(program, outer->machine_def.update_fn, tree->tree_lit_switch.pattern->value_ast, program->type_cache.word_uint_type);
            NecroMachSwitchTerminator* switch_value = necro_mach_build_switch(program, outer->machine_def.update_fn, value, NULL, error_block);
            for (size_t i = 0; i < tree->tree_lit_switch.num_cases; ++i)
            {
                if (tree->tree_lit_switch.constants[i].type == NECRO_AST_CONSTANT_STRING)
                {
                    // NOTE: WTF is up with using this to represent Wildcards / vars!?!?!
                    const char*   case_name  = "default";
                    NecroMachAst* case_block = necro_mach_block_insert_before(program, outer->machine_def.update_fn, case_name, term_case_block);
                    switch_value->else_block = case_block;
                    NecroBlockEnv* new_env   = necro_block_env_create(&program->arena, case_block, env);
                    necro_mach_block_move_to(program, outer->machine_def.update_fn, case_block);
                    necro_decision_tree_to_mach(program, tree->tree_lit_switch.cases[i], term_case_block, error_block, result_phi, outer, new_env);
                    tree->tree_lit_switch.cases[i]->block = case_block;
                    break;
                }
                else if (tree->tree_lit_switch.cases[i]->block != NULL)
                {
                    necro_mach_add_case_to_switch(program, switch_value, tree->tree_lit_switch.cases[i]->block, (size_t) tree->tree_lit_switch.constants[i].uint_literal);
                }
                else
                {
                    // NOTE / HACK: should proabably find a better method than 20 chars on the stack
                    char    int_buf[20] = { 0 };
                    // int64_t int_literal = tree->tree_lit_switch.constants[i].int_literal;
                    size_t uint_literal = (size_t) tree->tree_lit_switch.constants[i].uint_literal;
                    sprintf(int_buf, "%zu", uint_literal);
                    const char*    int_buf_addr = (const char*) int_buf;
                    const char*    case_name    = necro_snapshot_arena_concat_strings(&program->snapshot_arena, 2, (const char*[]) { int_buf_addr, "_case" });
                    NecroMachAst*  case_block   = necro_mach_block_insert_before(program, outer->machine_def.update_fn, case_name, term_case_block);
                    NecroBlockEnv* new_env      = necro_block_env_create(&program->arena, case_block, env);
                    necro_mach_add_case_to_switch(program, switch_value, case_block, (size_t) tree->tree_lit_switch.constants[i].int_literal);
                    necro_mach_block_move_to(program, outer->machine_def.update_fn, case_block);
                    necro_decision_tree_to_mach(program, tree->tree_lit_switch.cases[i], term_case_block, error_block, result_phi, outer, new_env);
                    tree->tree_lit_switch.cases[i]->block = case_block;
                }
            }
        }
        else
        {
            necro_decision_tree_to_mach(program, tree->tree_lit_switch.cases[0], term_case_block, error_block, result_phi, outer, env);
        }
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
    // TODO: Maximal sharing? Or not maximal sharing? Is it really that beneficial? Cost/Benefit analysis needed...
    // necro_decision_tree_print(program, tree);
    // tree                               = necro_decision_tree_maximal_sharing(tree);
    // necro_decision_tree_print(program, tree);

    if (!necro_decision_tree_is_branchless(tree))
    {
        // Branching
        NecroMachAst* current_block = necro_mach_block_get_current(outer->machine_def.update_fn);
        NecroMachAst* next_block    = current_block->block.next_block;

        // End block
        NecroMachAst*  term_case_block  = (next_block != NULL) ? necro_mach_block_insert_before(program, outer->machine_def.update_fn, "case_end", next_block) : necro_mach_block_append(program, outer->machine_def.update_fn, "case_end");
        necro_mach_block_move_to(program, outer->machine_def.update_fn, term_case_block);
        NecroMachType* result_type      = necro_mach_type_make_ptr_if_boxed(program, necro_mach_type_from_necro_type(program, ast->case_expr.alts->data->case_alt.expr->necro_type));
        NecroMachAst*  case_result      = necro_mach_build_phi(program, outer->machine_def.update_fn, result_type, NULL);
        // HACK
        NecroMachAst*  result_phi       = necro_mach_block_get_current(outer->machine_def.update_fn)->block.statements[necro_mach_block_get_current(outer->machine_def.update_fn)->block.num_statements - 1];

        // Error block
        NecroMachAst* err_block = outer->machine_def.update_fn->fn_def._err_block;
        if (err_block == NULL)
        {
            NecroMachAst* next_error_block = term_case_block->block.next_block;
            err_block = (next_error_block == NULL) ? necro_mach_block_append(program, outer->machine_def.update_fn, "error") : necro_mach_block_insert_before(program, outer->machine_def.update_fn, "error", next_error_block);
            necro_mach_block_move_to(program, outer->machine_def.update_fn, err_block);
            necro_mach_build_call(program, outer->machine_def.update_fn, program->runtime.necro_error_exit->ast->fn_def.fn_value, (NecroMachAst*[]) { necro_mach_value_create_uint32(program, 1) }, 1, NECRO_MACH_CALL_C, "");
            necro_mach_build_unreachable(program, outer->machine_def.update_fn);
            outer->machine_def.update_fn->fn_def._err_block = err_block;
        }

        // Build case in current block
        necro_mach_block_move_to(program, outer->machine_def.update_fn, current_block);
        NecroBlockEnv* env = necro_block_env_create(&program->arena, current_block, NULL);
        necro_decision_tree_to_mach(program, tree, term_case_block, err_block, result_phi, outer, env);
        necro_mach_block_move_to(program, outer->machine_def.update_fn, term_case_block);
        return case_result;
    }
    else
    {
        // Branchless
        NecroMachAst*  current_block = necro_mach_block_get_current(outer->machine_def.update_fn);
        NecroBlockEnv* env           = necro_block_env_create(&program->arena, current_block, NULL);
        NecroMachAst*  case_result   = necro_decision_tree_to_mach(program, tree, NULL, NULL, NULL, outer, env);
        return case_result;
    }
}

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


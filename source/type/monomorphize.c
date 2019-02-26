/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include "monomorphize.h"
#include "base.h"
#include "infer.h"
#include "result.h"
#include "kind.h"

/*
    Notes:
        * Crazy thoughts on Region based memory management:
            - Regions are large constant sized blocks of memory which are statically determined at compile time.
            - Regions are formed from all the memory required to form recursive values.
            - A region is allocated in one giant chunk and deallocated in one giant chunk.
            - To determine the size of the region we should put some (perhaps extreme) restrictions on the language:
                * No recursive functions
                * No recursive data types.
                * The language requires monomorphization.
                * Futhark type function restrictions (Can't be returned from an if statement, etc).
                * Recursive values must be statically initialized with a statically known initializer value.
            - Recursive value use cases:
                * Collections of objects (with list, etc),
                    Solution: use fixed size collections such as tuples or Arrays.
                * Associative collections:
                    Solution: ???
            - Recursive function use cases:
                * Functional equivalant of for-loop type construct (i.e mapping/folding over a list),
                    Solution: use regular values (with feedback for state is necessary) with (take/demand/?) function.
            - With the given restrictions all required memory for a region is statically known.
            - The runtime will then require:
                * A statically sized Region back buffer, to read from.
                * A statically sized Region front buffer, to write to.
                * A simple bump allocating temp buffer.

        * Need good story for fixed size arrays

        * Demand (or perhaps Lazy?) types, which have separate instantiations for each demand (or force?) site.

        * Default type class. Use this for polymorphic recursion
        * Given the above we could do an even more drastic memory scheme, such as large scale Region based memory management.
        * Rename pass to necro_type_specialize ???
        * Implement Patterns (The musical ones...) similarly to Iterator trait in Rust, via TypeClasses?

        * Allow recursive functions
        * Recursive functions behave as they currently do, as dynamic regions of memory
        * Allow functions as first class values, as currently, with stateful functions createing dynamic regions of memory
        * Use Region based memory management

    TODO:
        * Look into Pattern Assignment + Initializers
        * Check Numeric patterns
        * Pattern Literals
        * do statements
        * Prune pass after to cull all unneeded stuff and put it in a direct form to make Chad's job for Core translation easier?
*/

///////////////////////////////////////////////////////
// NecroMonomorphize
///////////////////////////////////////////////////////

typedef struct
{
    NecroIntern*         intern;
    NecroScopedSymTable* scoped_symtable;
    NecroBase*           base;
    NecroAstArena*       ast_arena;
    NecroPagedArena*     arena;
    NecroSnapshotArena   snapshot_arena;
    char*                suffix_buffer;
    size_t               suffix_size;
} NecroMonomorphize;

NecroMonomorphize necro_monomorphize_empty()
{
    NecroMonomorphize monomorphize = (NecroMonomorphize)
    {
        .intern          = NULL,
        .arena           = NULL,
        .snapshot_arena  = necro_snapshot_arena_empty(),
        .scoped_symtable = NULL,
        .base            = NULL,
        .ast_arena       = NULL,
    };
    return monomorphize;
}

NecroMonomorphize necro_monomorphize_create(NecroIntern* intern, NecroScopedSymTable* scoped_symtable, NecroBase* base, NecroAstArena* ast_arena)
{
    NecroMonomorphize monomorphize = (NecroMonomorphize)
    {
        .intern          = intern,
        .arena           = &ast_arena->arena,
        .snapshot_arena  = necro_snapshot_arena_create(),
        .scoped_symtable = scoped_symtable,
        .base            = base,
        .ast_arena       = ast_arena,
        .suffix_buffer   = malloc(256 * sizeof(char)),
        .suffix_size     = 256,
    };
    return monomorphize;
}

void necro_monomorphize_destroy(NecroMonomorphize* monomorphize)
{
    assert(monomorphize != NULL);
    necro_snapshot_arena_destroy(&monomorphize->snapshot_arena);
    free(monomorphize->suffix_buffer);
    *monomorphize = necro_monomorphize_empty();
}

///////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////
NecroResult(void) necro_monomorphize_go(NecroMonomorphize* monomorphize, NecroAst* ast, NecroInstSub* subs);
NecroAstSymbol*   necro_ast_specialize(NecroMonomorphize* monomorphize, NecroAstSymbol* ast_symbol, NecroInstSub* subs);


NecroResult(void) necro_monomorphize(NecroCompileInfo info, NecroIntern* intern, NecroScopedSymTable* scoped_symtable, NecroBase* base, NecroAstArena* ast_arena)
{
    NecroMonomorphize monomorphize = necro_monomorphize_create(intern, scoped_symtable, base, ast_arena);
    NecroResult(void) result       = necro_monomorphize_go(&monomorphize, ast_arena->root, NULL);
    necro_monomorphize_destroy(&monomorphize);
    if (result.type == NECRO_RESULT_OK)
    {
        if (info.compilation_phase == NECRO_PHASE_MONOMORPHIZE && info.verbosity > 0)
            necro_ast_arena_print(ast_arena);
        return ok_void();
    }
    return result;
}

///////////////////////////////////////////////////////
// SpecializeAst
///////////////////////////////////////////////////////
size_t necro_type_mangle_subs_recursive(char* suffix_buffer, size_t offset, const NecroInstSub* sub)
{
    if (sub == NULL)
        return offset;
    offset = necro_type_mangle_subs_recursive(suffix_buffer, offset, sub->next);
    if (sub->next != NULL)
        offset += sprintf(suffix_buffer + offset, ",");
    offset = necro_type_mangled_sprintf(suffix_buffer, offset, sub->new_name);
    return offset;
}

NecroSymbol necro_create_suffix_from_subs(NecroMonomorphize* monomorphize, NecroSymbol prefix, const NecroInstSub* subs)
{
    // Count length
    const size_t        prefix_length = strlen(prefix->str);
    size_t              length        = 2 + prefix_length;
    const NecroInstSub* curr_sub = subs;
    while (curr_sub != NULL)
    {
        length += necro_type_mangled_string_length(curr_sub->new_name);
        if (curr_sub->next != NULL)
            length += 1;
        curr_sub = curr_sub->next;
    }

    // Resize buffer
    if (monomorphize->suffix_size < length)
    {
        free(monomorphize->suffix_buffer);
        while (monomorphize->suffix_size < length)
            monomorphize->suffix_size *= 2;
        monomorphize->suffix_buffer = malloc(monomorphize->suffix_size * sizeof(char));
    }

    // Write type strings
    size_t offset = 0;
    if (prefix->str[prefix_length - 1] == '>')
    {
        // Name has already been overloaded for a type class, instead append to type list.
        offset = sprintf(monomorphize->suffix_buffer, "%s", prefix->str);
        monomorphize->suffix_buffer[prefix_length - 1] = ',';
        length -= 1;
    }
    else
    {
        offset = sprintf(monomorphize->suffix_buffer, "%s<", prefix->str);
    }
    offset        = necro_type_mangle_subs_recursive (monomorphize->suffix_buffer, offset, subs);
    offset       += sprintf(monomorphize->suffix_buffer + offset, ">");
    assert(offset == length);

    return necro_intern_string(monomorphize->intern, monomorphize->suffix_buffer);
}

NecroInstSub* necro_type_create_instance_subs(NecroMonomorphize* monomorphize, NecroTypeClass* type_class, NecroInstSub* subs)
{
    // Find Sub
    NecroInstSub* curr_sub = subs;
    while (curr_sub != NULL)
    {
        NecroType* sub_type = necro_type_find(curr_sub->new_name);
        if (curr_sub->var_to_replace == type_class->type_var)
        {
            // Find Instance
            assert(sub_type->type == NECRO_TYPE_CON);
            NecroInstanceList* instance_list = sub_type->con.con_symbol->instance_list;
            while (instance_list != NULL)
            {
                if (instance_list->data->type_class_name == type_class->type_class_name)
                {
                    // Create InstanceSubs
                    NecroTypeClassInstance* instance      = instance_list->data;
                    NecroInstSub*           instance_subs = NULL;
                    NecroType*              instance_type = unwrap(NecroType, necro_type_instantiate_with_subs(monomorphize->arena, monomorphize->base, instance->data_type, NULL, &instance_subs));
                    unwrap(NecroType, necro_type_unify(monomorphize->arena, monomorphize->base, instance_type, sub_type, NULL));
                    NecroInstSub*           new_subs      = necro_type_filter_and_deep_copy_subs(monomorphize->arena, subs, type_class->type_class_name, instance_type);
                    new_subs                              = necro_type_union_subs(new_subs, instance_subs);
                    if (new_subs != NULL)
                    {
                        NecroInstSub* curr_new_sub = new_subs;
                        while (curr_new_sub->next != NULL)
                        {
                            curr_new_sub = curr_new_sub->next;
                        }
                        curr_new_sub->next = instance_subs;
                        new_subs = necro_type_filter_and_deep_copy_subs(monomorphize->arena, new_subs, type_class->type_var, instance_type);
                        return new_subs;
                    }
                    else
                    {
                        instance_subs = necro_type_filter_and_deep_copy_subs(monomorphize->arena, instance_subs, type_class->type_class_name, instance_type);
                        return instance_subs;
                    }
                }
                instance_list = instance_list->next;
            }
            assert(false);
        }
        curr_sub = curr_sub->next;
    }
    assert(false);
    return NULL;
}

NecroAstSymbol* necro_ast_specialize_method(NecroMonomorphize* monomorphize, NecroAstSymbol* ast_symbol, NecroInstSub* subs)
{
    assert(monomorphize != NULL);
    assert(ast_symbol != NULL);
    assert(subs != NULL);

    NecroTypeClass* type_class     = ast_symbol->method_type_class;
    NecroAstSymbol* type_class_var = type_class->type_var;
    NecroInstSub*   curr_sub       = subs;
    while (curr_sub != NULL)
    {
        if (curr_sub->var_to_replace->name == type_class_var->name)
        {
            NecroType* sub_con = necro_type_find(curr_sub->new_name);
            assert(sub_con->type == NECRO_TYPE_CON);
            NecroSymbol     instance_method_name       = necro_intern_create_type_class_instance_symbol(monomorphize->intern, ast_symbol->source_name, sub_con->con.con_symbol->source_name);
            NecroAstSymbol* instance_method_ast_symbol = necro_scope_find_ast_symbol(monomorphize->scoped_symtable->top_scope, instance_method_name);
            assert(instance_method_ast_symbol != NULL);
            if (necro_type_is_polymorphic(instance_method_ast_symbol->type))
            {
                NecroInstSub* instance_subs = necro_type_create_instance_subs(monomorphize, type_class, subs);
                return necro_ast_specialize(monomorphize, instance_method_ast_symbol, instance_subs);
            }
            else
            {
                return instance_method_ast_symbol;
            }
        }
        curr_sub = curr_sub->next;
    }
    assert(false);
    return NULL;
}

NecroAstSymbol* necro_ast_specialize(NecroMonomorphize* monomorphize, NecroAstSymbol* ast_symbol, NecroInstSub* subs)
{
    assert(monomorphize != NULL);
    assert(ast_symbol != NULL);
    assert(ast_symbol->ast != NULL);
    assert(ast_symbol->ast->scope != NULL);
    assert(subs != NULL);

    // TODO: Assert sub size == for_all size
    if (ast_symbol->method_type_class)
        return necro_ast_specialize_method(monomorphize, ast_symbol, subs);

    //--------------------
    // Find specialized ast
    //--------------------
    NecroSymbol     specialized_name       = necro_create_suffix_from_subs(monomorphize, ast_symbol->source_name, subs);
    NecroScope*     scope                  = ast_symbol->ast->scope;
    NecroAstSymbol* specialized_ast_symbol = necro_scope_find_ast_symbol(scope, specialized_name);
    if (specialized_ast_symbol != NULL)
    {
        assert(specialized_ast_symbol->ast != NULL);
        return specialized_ast_symbol;
    }

    //--------------------
    // Find DeclarationGroupList, Make new DeclarationGroup
    //--------------------
    NecroAst* declaration_group = ast_symbol->declaration_group;

    assert(declaration_group != NULL);
    assert(declaration_group->type == NECRO_AST_DECL);
    NecroAst* new_declaration                           = necro_ast_create_decl(monomorphize->arena, ast_symbol->ast, declaration_group->declaration.next_declaration);
    declaration_group->declaration.next_declaration     = new_declaration;
    new_declaration->declaration.declaration_group_list = declaration_group->declaration.declaration_group_list;
    new_declaration->declaration.declaration_impl       = NULL; // Removing dummy argument

    //--------------------
    // Deep Copy Ast
    //--------------------
    specialized_ast_symbol                        = necro_ast_symbol_create(monomorphize->arena, specialized_name, specialized_name, monomorphize->ast_arena->module_name, NULL);
    specialized_ast_symbol->declaration_group     = new_declaration;
    specialized_ast_symbol->ast                   = necro_ast_deep_copy_go(monomorphize->arena, new_declaration, ast_symbol->ast);
    new_declaration->declaration.declaration_impl = specialized_ast_symbol->ast;
    assert(ast_symbol->ast != specialized_ast_symbol->ast);
    switch (specialized_ast_symbol->ast->type)
    {
    case NECRO_AST_SIMPLE_ASSIGNMENT:
        specialized_ast_symbol->ast->simple_assignment.ast_symbol = specialized_ast_symbol;
        break;
    case NECRO_AST_APATS_ASSIGNMENT:
        specialized_ast_symbol->ast->apats_assignment.ast_symbol = specialized_ast_symbol;
        break;
    default:
        assert(false);
        break;
    }
    NecroScope* prev_scope                                    = monomorphize->scoped_symtable->current_scope;
    NecroScope* prev_type_scope                               = monomorphize->scoped_symtable->current_type_scope;
    monomorphize->scoped_symtable->current_scope      = scope;
    necro_rename_internal_scope_and_rename(monomorphize->ast_arena, monomorphize->scoped_symtable, monomorphize->intern, specialized_ast_symbol->ast);
    monomorphize->scoped_symtable->current_scope      = prev_scope;
    monomorphize->scoped_symtable->current_type_scope = prev_type_scope;

    //--------------------
    // Specialize Ast
    //--------------------
    specialized_ast_symbol->type = unwrap(NecroType, necro_type_replace_with_subs_deep_copy(monomorphize->arena, monomorphize->base, ast_symbol->type, subs));
    unwrap(void, necro_monomorphize_go(monomorphize, specialized_ast_symbol->ast, subs));

    return specialized_ast_symbol;
}

/*
    Note: To specialize a value or function:
        * The value or function must be polymorphic.
        * The usage of the value or function must be completely concrete.
        * If we encounter a monomorphic type variable we try to default it, if we can't it's a type error.
*/
NecroResult(bool) necro_ast_should_specialize(NecroPagedArena* arena, NecroBase* base, NecroAstSymbol* ast_symbol, NecroAst* ast, NecroInstSub* inst_subs)
{
    const bool is_symbol_poly    = necro_try(bool, necro_type_is_unambiguous_polymorphic(arena, base, ast_symbol->type, ast_symbol->type, ast->source_loc, ast->end_loc));
    const bool is_ast_poly       = necro_try(bool, necro_type_is_unambiguous_polymorphic(arena, base, ast->necro_type, ast->necro_type, ast->source_loc, ast->end_loc));
    bool       should_specialize = is_symbol_poly && !is_ast_poly;
    NecroInstSub* curr_sub    = inst_subs;
    while (curr_sub != NULL)
    {
        const bool is_curr_sub_poly = necro_try(bool, necro_type_is_unambiguous_polymorphic(arena, base, curr_sub->new_name, curr_sub->new_name, ast->source_loc, ast->end_loc));
        should_specialize           = should_specialize && !is_curr_sub_poly;
        curr_sub                    = curr_sub->next;
    }
    return ok(bool, should_specialize);
}

NecroResult(void) necro_rec_check_pat_assignment(NecroBase* base, NecroAst* ast)
{
    assert(ast != NULL);
    switch (ast->type)
    {
    case NECRO_AST_VARIABLE:
        if (ast->variable.initializer != NULL && !necro_is_fully_concrete(base, ast->variable.ast_symbol->type))
        {
            return necro_type_non_concrete_initialized_value_error(ast->simple_assignment.ast_symbol, ast->necro_type, ast->source_loc, ast->end_loc);
        }
        if (ast->variable.initializer != NULL && !ast->variable.is_recursive)
        {
            return necro_type_non_recursive_initialized_value_error(ast->simple_assignment.ast_symbol, ast->necro_type, ast->source_loc, ast->end_loc);
        }
        return ok_void();
    case NECRO_AST_CONSTANT:
        return ok_void();
    case NECRO_AST_TUPLE:
    {
        NecroAst* args = ast->tuple.expressions;
        while (args != NULL)
        {
            necro_try(void, necro_rec_check_pat_assignment(base, args->list.item));
            args = args->list.next_item;
        }
        return ok_void();
    }
    case NECRO_AST_EXPRESSION_LIST:
    {
        NecroAst* args = ast->expression_list.expressions;
        while (args != NULL)
        {
            necro_try(void, necro_rec_check_pat_assignment(base, args->list.item));
            args = args->list.next_item;
        }
        return ok_void();
    }
    case NECRO_AST_WILDCARD:
        return ok_void();
    case NECRO_AST_BIN_OP_SYM:
        necro_try(void, necro_rec_check_pat_assignment(base, ast->bin_op_sym.left));
        return necro_rec_check_pat_assignment(base, ast->bin_op_sym.right);
    case NECRO_AST_CONID:
        return ok_void();
    case NECRO_AST_CONSTRUCTOR:
    {
        NecroAst* args = ast->constructor.arg_list;
        while (args != NULL)
        {
            necro_try(void, necro_rec_check_pat_assignment(base, args->list.item));
            args = args->list.next_item;
        }
        return ok_void();
    }
    default:
        necro_unreachable(void);
    }
}

///////////////////////////////////////////////////////
// Translate Go
///////////////////////////////////////////////////////
NecroResult(void) necro_monomorphize_go(NecroMonomorphize* monomorphize, NecroAst* ast, NecroInstSub* subs)
{
    if (ast == NULL)
        return ok_void();
    ast->necro_type = necro_try_map(NecroType, void, necro_type_replace_with_subs(monomorphize->arena, monomorphize->base, ast->necro_type, subs));
    switch (ast->type)
    {

    //=====================================================
    // Declaration type things
    //=====================================================
    case NECRO_AST_TOP_DECL:
        necro_unreachable(void);

    case NECRO_AST_DECLARATION_GROUP_LIST:
    {
        NecroAst* group_list = ast;
        while (group_list != NULL)
        {
            necro_try(void, necro_monomorphize_go(monomorphize, group_list->declaration_group_list.declaration_group, subs));
            group_list = group_list->declaration_group_list.next;
        }
        return ok_void();
    }

    case NECRO_AST_DECL:
    {
        NecroAst* declaration_group = ast;
        while (declaration_group != NULL)
        {
            necro_try(void, necro_monomorphize_go(monomorphize, declaration_group->declaration.declaration_impl, subs));
            declaration_group = declaration_group->declaration.next_declaration;
        }
        return ok_void();
    }

    case NECRO_AST_TYPE_CLASS_INSTANCE:
        return necro_monomorphize_go(monomorphize, ast->type_class_instance.declarations, subs);

    //=====================================================
    // Assignment type things
    //=====================================================
    case NECRO_AST_SIMPLE_ASSIGNMENT:
        ast->simple_assignment.ast_symbol->type = necro_try_map(NecroType, void, necro_type_replace_with_subs(monomorphize->arena, monomorphize->base, ast->simple_assignment.ast_symbol->type, subs));
        if (ast->simple_assignment.initializer != NULL && !necro_is_fully_concrete(monomorphize->base, ast->necro_type))
        {
            return necro_type_non_concrete_initialized_value_error(ast->simple_assignment.ast_symbol, ast->necro_type, ast->source_loc, ast->end_loc);
        }
        if (ast->simple_assignment.initializer != NULL && !ast->simple_assignment.is_recursive)
        {
            return necro_type_non_recursive_initialized_value_error(ast->simple_assignment.ast_symbol, ast->necro_type, ast->source_loc, ast->end_loc);
        }
        necro_try(void, necro_monomorphize_go(monomorphize, ast->simple_assignment.initializer, subs));
        return necro_monomorphize_go(monomorphize, ast->simple_assignment.rhs, subs);

    case NECRO_AST_APATS_ASSIGNMENT:
        ast->apats_assignment.ast_symbol->type = necro_try_map(NecroType, void, necro_type_replace_with_subs(monomorphize->arena, monomorphize->base, ast->apats_assignment.ast_symbol->type, subs));
        necro_try(void, necro_monomorphize_go(monomorphize, ast->apats_assignment.apats, subs));
        return necro_monomorphize_go(monomorphize, ast->apats_assignment.rhs, subs);

    case NECRO_AST_PAT_ASSIGNMENT:
        necro_try(void, necro_rec_check_pat_assignment(monomorphize->base, ast->pat_assignment.pat));
        necro_try(void, necro_monomorphize_go(monomorphize, ast->pat_assignment.pat, subs));
        return necro_monomorphize_go(monomorphize, ast->pat_assignment.rhs, subs);

    case NECRO_AST_DATA_DECLARATION:
        // necro_try(void, necro_monomorphize_go(monomorphize, ast->data_declaration.simpletype, subs));
        // return necro_monomorphize_go(monomorphize, ast->data_declaration.constructor_list, subs);
        return ok_void();

    case NECRO_AST_VARIABLE:
        if (ast->variable.ast_symbol == monomorphize->base->prim_undefined)
            return ok_void();
        switch (ast->variable.var_type)
        {
        case NECRO_VAR_VAR:
        {
            ast->variable.inst_subs      = necro_type_union_subs(ast->variable.inst_subs, subs);
            const bool should_specialize = necro_try_map(bool, void, necro_ast_should_specialize(monomorphize->arena, monomorphize->base, ast->variable.ast_symbol, ast, ast->variable.inst_subs));
            if (!should_specialize)
                return ok_void();
            NecroAstSymbol* specialized_ast_symbol = necro_ast_specialize(monomorphize, ast->variable.ast_symbol, ast->variable.inst_subs);
            NecroAst*       specialized_var        = necro_ast_create_var_full(monomorphize->arena, specialized_ast_symbol, NECRO_VAR_VAR, NULL, NULL);
            specialized_var->necro_type            = specialized_ast_symbol->type;
            *ast = *specialized_var;
            return ok_void();
        }

        case NECRO_VAR_DECLARATION:
            if (ast->variable.ast_symbol != NULL)
            {
                ast->variable.ast_symbol->type = necro_try_map(NecroType, void, necro_type_replace_with_subs(monomorphize->arena, monomorphize->base, ast->variable.ast_symbol->type, subs));
            }
            if (ast->variable.initializer != NULL)
                necro_try(void, necro_monomorphize_go(monomorphize, ast->variable.initializer, subs));
            return ok_void();
        case NECRO_VAR_SIG:                  return ok_void();
        case NECRO_VAR_TYPE_VAR_DECLARATION: return ok_void();
        case NECRO_VAR_TYPE_FREE_VAR:        return ok_void();
        case NECRO_VAR_CLASS_SIG:            return ok_void();
        default:
            necro_unreachable(void);
        }

    case NECRO_AST_CONID:
        necro_try(void, necro_type_ambiguous_type_variable_check(monomorphize->arena, monomorphize->base, ast->necro_type, ast->necro_type, ast->source_loc, ast->end_loc));
        return ok_void();

    case NECRO_AST_UNDEFINED:
        return ok_void();

    case NECRO_AST_CONSTANT:
        // TODO (Curtis, 2-14-19): Handle this for overloaded numeric literals in patterns
        necro_try(void, necro_type_ambiguous_type_variable_check(monomorphize->arena, monomorphize->base, ast->necro_type, ast->necro_type, ast->source_loc, ast->end_loc));
        return ok_void();

    case NECRO_AST_UN_OP:
        necro_try(void, necro_type_ambiguous_type_variable_check(monomorphize->arena, monomorphize->base, ast->necro_type, ast->necro_type, ast->source_loc, ast->end_loc));
        return ok_void();

    case NECRO_AST_BIN_OP:
    {
        ast->bin_op.inst_subs        = necro_type_union_subs(ast->bin_op.inst_subs, subs);
        const bool should_specialize = necro_try_map(bool, void, necro_ast_should_specialize(monomorphize->arena, monomorphize->base, ast->bin_op.ast_symbol, ast, ast->bin_op.inst_subs));
        if (should_specialize)
        {
            ast->bin_op.ast_symbol = necro_ast_specialize(monomorphize, ast->bin_op.ast_symbol, ast->bin_op.inst_subs);
        }
        necro_try(void, necro_monomorphize_go(monomorphize, ast->bin_op.lhs, subs));
        return necro_monomorphize_go(monomorphize, ast->bin_op.rhs, subs);
    }

    case NECRO_AST_OP_LEFT_SECTION:
    {
        ast->op_left_section.inst_subs = necro_type_union_subs(ast->op_left_section.inst_subs, subs);
        const bool should_specialize   = necro_try_map(bool, void, necro_ast_should_specialize(monomorphize->arena, monomorphize->base, ast->op_left_section.ast_symbol, ast, ast->op_left_section.inst_subs));
        if (should_specialize)
        {
            ast->op_left_section.ast_symbol = necro_ast_specialize(monomorphize, ast->op_left_section.ast_symbol, ast->op_left_section.inst_subs);
        }
        return necro_monomorphize_go(monomorphize, ast->op_left_section.left, subs);
    }

    case NECRO_AST_OP_RIGHT_SECTION:
    {
        ast->op_right_section.inst_subs = necro_type_union_subs(ast->op_right_section.inst_subs, subs);
        const bool should_specialize    = necro_try_map(bool, void, necro_ast_should_specialize(monomorphize->arena, monomorphize->base, ast->op_right_section.ast_symbol, ast, ast->op_right_section.inst_subs));
        if (should_specialize)
        {
            ast->op_right_section.ast_symbol = necro_ast_specialize(monomorphize, ast->op_right_section.ast_symbol, ast->op_right_section.inst_subs);
        }
        return necro_monomorphize_go(monomorphize, ast->op_right_section.right, subs);
    }

    case NECRO_AST_IF_THEN_ELSE:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->if_then_else.if_expr, subs));
        necro_try(void, necro_monomorphize_go(monomorphize, ast->if_then_else.then_expr, subs));
        return necro_monomorphize_go(monomorphize, ast->if_then_else.else_expr, subs);

    case NECRO_AST_RIGHT_HAND_SIDE:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->right_hand_side.declarations, subs));
        return necro_monomorphize_go(monomorphize, ast->right_hand_side.expression, subs);

    case NECRO_AST_LET_EXPRESSION:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->let_expression.declarations, subs));
        return necro_monomorphize_go(monomorphize, ast->let_expression.expression, subs);

    case NECRO_AST_FUNCTION_EXPRESSION:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->fexpression.next_fexpression, subs));
        return necro_monomorphize_go(monomorphize, ast->fexpression.aexp, subs);

    case NECRO_AST_APATS:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->apats.apat, subs));
        return necro_monomorphize_go(monomorphize, ast->apats.next_apat, subs);

    case NECRO_AST_WILDCARD:
        return ok_void();

    case NECRO_AST_LAMBDA:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->lambda.apats, subs));
        return necro_monomorphize_go(monomorphize, ast->lambda.expression, subs);

//     case NECRO_AST_DO:
//     {
           // // TODO: update ast_symbol type for bind!
           // // ast->bin_op.ast_symbol->type  = necro_type_replace_with_subs(monomorphize->arena, ast->bin_op.ast_symbol->type, subs);
//         // DO statements NOT FULLY IMPLEMENTED currently
//         necro_unreachable(NecroType);
//         // TODO: REDO!
//         // NecroAst* bind_node = necro_ast_create_var(infer->arena, infer->intern, "bind", NECRO_VAR_VAR);
//         // NecroScope* scope = ast->scope;
//         // while (scope->parent != NULL)
//         //     scope = scope->parent;
//         // bind_node->scope = bind_node->scope = scope;
//         // // TODO: NOTE THIS NEEDS TO BE REPLACED WITH NEW SYSTEM THAT DOESN'T USE THE RENAMER!
//         // // necro_rename_var_pass(infer->renamer, infer->arena, bind_node);
//         // bind_node->necro_type = necro_symtable_get(infer->symtable, bind_node->variable.id)->type;
//         // NecroTypeClassContext* bind_context     = necro_symtable_get(infer->symtable, bind_node->variable.id)->type->for_all.context;
//         // NecroTypeClassContext* monad_context    = ast->do_statement.monad_var->var.context;
//         // while (monad_context->type_class_name.id.id != bind_context->type_class_name.id.id)
//         //     monad_context = monad_context->next;
//         // // assert(monad_context->type_class_name.id.id != bind_context->type_class_name.id.id);
//         // NecroAst*          bind_method_inst = necro_resolve_method(infer, monad_context->type_class, monad_context, bind_node, dictionary_context);
//         // necro_ast_print(bind_method_inst);
//         // necro_monomorphize_go(monomorphize, ast->do_statement.statement_list);
//         // break;
//     }

    case NECRO_AST_LIST_NODE:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->list.item, subs));
        return necro_monomorphize_go(monomorphize, ast->list.next_item, subs);

    case NECRO_AST_EXPRESSION_LIST:
        return necro_monomorphize_go(monomorphize, ast->expression_list.expressions, subs);

    case NECRO_AST_EXPRESSION_ARRAY:
        return necro_monomorphize_go(monomorphize, ast->expression_array.expressions, subs);

    case NECRO_AST_PAT_EXPRESSION:
        return necro_monomorphize_go(monomorphize, ast->pattern_expression.expressions, subs);

    case NECRO_AST_TUPLE:
        return necro_monomorphize_go(monomorphize, ast->tuple.expressions, subs);

    case NECRO_BIND_ASSIGNMENT:
        necro_try(void, necro_type_ambiguous_type_variable_check(monomorphize->arena, monomorphize->base, ast->necro_type, ast->necro_type, ast->source_loc, ast->end_loc));
        ast->bind_assignment.ast_symbol->type = necro_try_map(NecroType, void, necro_type_replace_with_subs(monomorphize->arena, monomorphize->base, ast->bind_assignment.ast_symbol->type, subs));
        return necro_monomorphize_go(monomorphize, ast->bind_assignment.expression, subs);

    case NECRO_PAT_BIND_ASSIGNMENT:
        necro_try(void, necro_type_ambiguous_type_variable_check(monomorphize->arena, monomorphize->base, ast->necro_type, ast->necro_type, ast->source_loc, ast->end_loc));
        necro_try(void, necro_monomorphize_go(monomorphize, ast->pat_bind_assignment.pat, subs));
        return necro_monomorphize_go(monomorphize, ast->pat_bind_assignment.expression, subs);

    case NECRO_AST_ARITHMETIC_SEQUENCE:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->arithmetic_sequence.from, subs));
        necro_try(void, necro_monomorphize_go(monomorphize, ast->arithmetic_sequence.then, subs));
        return necro_monomorphize_go(monomorphize, ast->arithmetic_sequence.to, subs);

    case NECRO_AST_CASE:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->case_expression.expression, subs));
        return necro_monomorphize_go(monomorphize, ast->case_expression.alternatives, subs);

    case NECRO_AST_CASE_ALTERNATIVE:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->case_alternative.pat, subs));
        return necro_monomorphize_go(monomorphize, ast->case_alternative.body, subs);

    case NECRO_AST_TYPE_APP:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->type_app.ty, subs));
        return necro_monomorphize_go(monomorphize, ast->type_app.next_ty, subs);

    case NECRO_AST_BIN_OP_SYM:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->bin_op_sym.left, subs));
        necro_try(void, necro_monomorphize_go(monomorphize, ast->bin_op_sym.op, subs));
        return necro_monomorphize_go(monomorphize, ast->bin_op_sym.right, subs);

    case NECRO_AST_CONSTRUCTOR:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->constructor.conid, subs));
        return necro_monomorphize_go(monomorphize, ast->constructor.arg_list, subs);

    case NECRO_AST_SIMPLE_TYPE:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->simple_type.type_con, subs));
        return necro_monomorphize_go(monomorphize, ast->simple_type.type_var_list, subs);

    case NECRO_AST_TYPE_CLASS_DECLARATION:
    case NECRO_AST_TYPE_SIGNATURE:
    case NECRO_AST_TYPE_CLASS_CONTEXT:
    case NECRO_AST_FUNCTION_TYPE:
        return ok_void();
    default:
        necro_unreachable(void);
    }
}

///////////////////////////////////////////////////////
// Testing
///////////////////////////////////////////////////////
void necro_monomorphize_test_result(const char* test_name, const char* str, NECRO_RESULT_TYPE expected_result, const NECRO_RESULT_ERROR_TYPE* error_type)
{
    // Set up
    NecroIntern         intern          = necro_intern_create();
    NecroSymTable       symtable        = necro_symtable_create(&intern);
    NecroScopedSymTable scoped_symtable = necro_scoped_symtable_create(&symtable);
    NecroBase           base            = necro_base_compile(&intern, &scoped_symtable);

    NecroLexTokenVector tokens          = necro_empty_lex_token_vector();
    NecroParseAstArena  parse_ast       = necro_parse_ast_arena_empty();
    NecroAstArena       ast             = necro_ast_arena_empty();
    NecroCompileInfo    info            = necro_test_compile_info();

    // Compile
    unwrap(void, necro_lex(info, &intern, str, strlen(str), &tokens));
    unwrap(void, necro_parse(info, &intern, &tokens, necro_intern_string(&intern, "Test"), &parse_ast));
    ast = necro_reify(info, &intern, &parse_ast);
    necro_build_scopes(info, &scoped_symtable, &ast);
    unwrap(void, necro_rename(info, &scoped_symtable, &intern, &ast));
    necro_dependency_analyze(info, &intern, &ast);
    unwrap(void, necro_infer(info, &intern, &scoped_symtable, &base, &ast));
    NecroResult(void) result = necro_monomorphize(info, &intern, &scoped_symtable, &base, &ast);
    // NecroResult(void) result = ok_void();

    // Assert
    // if (result.type != expected_result)
    // {
        // necro_ast_arena_print(&base.ast);
        necro_ast_arena_print(&ast);
        necro_scoped_symtable_print_top_scopes(&scoped_symtable);
    // }
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
    printf("Infer %s test: %s\n", test_name, result_string);
    fflush(stdout);

    // Clean up
    if (result.type == NECRO_RESULT_ERROR)
        necro_result_error_print(result.error, str, "Test");
    else if (result.error)
        free(result.error);

    necro_ast_arena_destroy(&ast);
    necro_base_destroy(&base);
    necro_parse_ast_arena_destroy(&parse_ast);
    necro_destroy_lex_token_vector(&tokens);
    necro_scoped_symtable_destroy(&scoped_symtable);
    necro_symtable_destroy(&symtable);
    necro_intern_destroy(&intern);
}

void necro_monomorphize_test_suffix()
{
    // Set up
    NecroIntern         intern          = necro_intern_create();
    NecroSymTable       symtable        = necro_symtable_create(&intern);
    NecroScopedSymTable scoped_symtable = necro_scoped_symtable_create(&symtable);
    NecroBase           base            = necro_base_compile(&intern, &scoped_symtable);

    NecroMonomorphize translate = necro_monomorphize_create(&intern, &scoped_symtable, &base, &base.ast);
    NecroSymbol             prefix    = necro_intern_string(&intern, "superCool");

    NecroInstSub* subs =
        necro_create_inst_sub_manual(&base.ast.arena, NULL,
            base.unit_type->type,
                necro_create_inst_sub_manual(&base.ast.arena, NULL,
                    base.unit_type->type, NULL));
    NecroSymbol suffix_symbol = necro_create_suffix_from_subs(&translate, prefix, subs);
    printf("suffix1: %s\n", suffix_symbol->str);


    NecroInstSub* subs2 =
        necro_create_inst_sub_manual(&base.ast.arena, NULL,
            base.mouse_x_fn->type,
                necro_create_inst_sub_manual(&base.ast.arena, NULL,
                    base.unit_type->type, NULL));
    NecroSymbol suffix_symbol2 = necro_create_suffix_from_subs(&translate, prefix, subs2);
    printf("suffix2: %s\n", suffix_symbol2->str);

    NecroInstSub* subs3 =
        necro_create_inst_sub_manual(&base.ast.arena, NULL,
            necro_type_fn_create(&base.ast.arena,
                necro_type_con_create(&base.ast.arena, base.maybe_type, necro_type_list_create(&base.ast.arena, base.int_type->type, NULL)),
                necro_type_con_create(&base.ast.arena, base.maybe_type, necro_type_list_create(&base.ast.arena, base.float_type->type, NULL))),
            NULL);
    NecroSymbol suffix_symbol3 = necro_create_suffix_from_subs(&translate, prefix, subs3);
    printf("suffix3: %s\n", suffix_symbol3->str);

    NecroInstSub* subs4 =
        necro_create_inst_sub_manual(&base.ast.arena, NULL,
            necro_type_con_create(&base.ast.arena, base.tuple2_type, necro_type_list_create(&base.ast.arena, base.int_type->type, necro_type_list_create(&base.ast.arena, base.float_type->type, NULL))),
            NULL);
    NecroSymbol suffix_symbol4 = necro_create_suffix_from_subs(&translate, prefix, subs4);
    printf("suffix4: %s\n", suffix_symbol4->str);

}

void necro_monomorphize_test()
{
    // necro_monomorphize_test_suffix();

    {
        const char* test_name   = "SimplePoly1";
        const char* test_source = ""
            "polyNothing :: Maybe a\n"
            "polyNothing = Nothing\n"
            "concreteNothing :: Maybe Int\n"
            "concreteNothing = polyNothing\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "SimplePoly2";
        const char* test_source = ""
            "polyNothing :: Maybe a\n"
            "polyNothing = Nothing\n"
            "concreteNothing :: Maybe Int\n"
            "concreteNothing = polyNothing\n"
            "concreteNothing2 :: Maybe Audio\n"
            "concreteNothing2 = polyNothing\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "SimplePoly3";
        const char* test_source = ""
            "polyNothing :: Maybe a\n"
            "polyNothing = Nothing\n"
            "concreteTuple :: (Maybe Bool, Maybe Audio)\n"
            "concreteTuple = (polyNothing, polyNothing)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "DoublePoly1";
        const char* test_source = ""
            "polyTuple :: a -> b -> (a, b) \n"
            "polyTuple x y = (x, y)\n"
            "concreteTuple :: ((), Bool)\n"
            "concreteTuple = polyTuple () False\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Method1";
        const char* test_source = ""
            "getReal :: Float\n"
            "getReal = 0\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Method2";
        const char* test_source = ""
            "unreal :: Audio\n"
            "unreal = 10 * 440 / 3\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Context1";
        const char* test_source = ""
            "inContext :: Num a => a -> a -> a\n"
            "inContext x y = x + y * 1000\n"
            "outOfContext :: Int\n"
            "outOfContext = inContext 10 99\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Left Section";
        const char* test_source = ""
            "left :: Int -> Int \n"
            "left = (10+)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Right Section";
        const char* test_source = ""
            "right :: Rational -> Rational \n"
            "right = (*33.3)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Context2";
        const char* test_source = ""
            "numbernomicon :: Num a => a -> a -> a\n"
            "numbernomicon x y = x + y * 1000\n"
            "atTheIntOfMadness :: Int\n"
            "atTheIntOfMadness = numbernomicon 0 300\n"
            "audioOutOfSpace :: Audio\n"
            "audioOutOfSpace = numbernomicon 10 99\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Context3";
        const char* test_source = ""
            "equalizer :: (Num a, Eq a) => a -> a -> Bool\n"
            "equalizer x y = x + y == x\n"
            "zero :: Int\n"
            "zero = 0\n"
            "oneHundred :: Int\n"
            "oneHundred = 1000\n"
            "notTheSame :: Bool\n"
            "notTheSame = equalizer zero oneHundred\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "DoubleContext";
        const char* test_source = ""
            "doubleTrouble :: (Num a, Num b) => a -> b -> (a, b)\n"
            "doubleTrouble x y = (x - 1000, y + 1000)\n"
            "doubleEdge :: (Rational, Float)\n"
            "doubleEdge = doubleTrouble 22 33\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "HalfContext";
        const char* test_source = ""
            "halfTrouble :: Num a => a -> Int -> (a, Int)\n"
            "halfTrouble x y = (x - 1000, y + 1000)\n"
            "halfEdge :: (Audio, Int)\n"
            "halfEdge = halfTrouble 22.2 33\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "DeepContext";
        const char* test_source = ""
            "deep :: Fractional a => a -> a\n"
            "deep x = x / x\n"
            "shallow :: (Fractional a, Fractional b) => a -> b -> (a, b)\n"
            "shallow y z = (deep y, deep z)\n"
            "top :: (Rational, Float)\n"
            "top = shallow 22.2 33.3\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "No Monomorphism Restriction";
        const char* test_source = ""
            "data Alive = Alive\n"
            "data Dead  = Dead\n"
            "cat :: (Maybe Alive, Maybe Dead)\n"
            "cat = (alive, dead) where\n"
            "  schroedingersMaybe = Nothing\n"
            "  alive              = schroedingersMaybe\n"
            "  dead               = schroedingersMaybe\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "No Monomorphism Restriction 2";
        const char* test_source = ""
            "notMonoFucker = add\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Polymorphic Data Type 1";
        const char* test_source = ""
            "data Thing a = Thing a\n"
            "addThing :: Num a => Thing a -> Thing a -> Thing a\n"
            "addThing (Thing x) (Thing y) = Thing (x + y)\n"
            "thingInt :: Num a => Int -> Thing a\n"
            "thingInt x = Thing (fromInt x)\n"
            "intThing :: Thing Int\n"
            "intThing = addThing (thingInt 1) (thingInt 2)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Polymorphic Data Type 2";
        const char* test_source = ""
            "tupleAdd :: (Num a, Num b) => (a, b) -> (a, b) -> (a, b)\n"
            "tupleAdd (xa, xb) (ya, yb) = (xa + ya, xb + yb)\n"
            "tupleFromInt :: (Num a, Num b) => Int -> (a, b)\n"
            "tupleFromInt x = (fromInt x, fromInt x)\n"
            "tupleIntFloat :: (Int, Float)\n"
            "tupleIntFloat = tupleAdd (tupleFromInt 1) (tupleFromInt 2)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Polymorphic methods 1";
        const char* test_source = ""
            "data Thing a = Thing a\n"
            "instance Num a => Num (Thing a) where\n"
            "  add (Thing x) (Thing y) = Thing (x + y)\n"
            "  sub (Thing x) (Thing y) = Thing (x - y)\n"
            "  mul (Thing x) (Thing y) = Thing (x * y)\n"
            "  abs (Thing x)           = Thing (abs x)\n"
            "  signum (Thing x)        = Thing (signum x)\n"
            "  fromInt x               = Thing (fromInt x)\n"
            "intThing :: Thing Int\n"
            "intThing = 10 + 33 * 3\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Instance Declarations 2";
        const char* test_source = ""
            "class NumCollection c where\n"
            "  checkOutMyCollection :: Num a => a -> c a\n"
            "instance NumCollection [] where\n"
            "  checkOutMyCollection x = [x + 1]\n"
            "rationalCollection :: [Rational]\n"
            "rationalCollection = checkOutMyCollection 22\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Polymorphic methods 1";
        const char* test_source = ""
            "audioPat :: Pattern Audio\n"
            "audioPat = 3.3 / 4 * 22.2\n"
            "floatPat :: Pattern Float\n"
            "floatPat = 3.3 / 4 * 22.2\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }
    {
        const char* test_name   = "Instance Chains 1";
        const char* test_source = ""
            "class UberClass u where\n"
            "  uberMensch :: Num n => u -> n\n"
            "class (UberClass s) => SuperClass s where\n"
            "  superMensch :: Num n => s -> n\n"
            "class (SuperClass c) => NoClass c where\n"
            "  mensch :: Num n => c -> n\n"
            "instance UberClass Int where\n"
            "  uberMensch x = fromInt x\n"
            "instance SuperClass Int where\n"
            "  superMensch x = uberMensch x\n"
            "instance NoClass Int where\n"
            "  mensch x = superMensch x\n"
            "iAmNotAMonster :: Float\n"
            "iAmNotAMonster = mensch 1\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Defaulting 1";
        const char* test_source = ""
            "const :: a -> b -> a\n"
            "const x y = x\n"
            "amb :: Audio\n"
            "amb = const 22 33.0\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Defaulting 2";
        const char* test_source = ""
            "eqconst :: Eq b => a -> Maybe b -> a\n"
            "eqconst x y = x\n"
            "amb :: Bool\n"
            "amb = eqconst True Nothing\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Defaulting 3";
        const char* test_source = ""
            "equalizer :: (Eq a) => a -> a -> Bool\n"
            "equalizer x y = x == x\n"
            "notTheSame :: Bool\n"
            "notTheSame = equalizer 0.5 100\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Defaulting 4";
        const char* test_source = ""
            "class NotUnit a where\n"
            "equalizer :: (Eq a) => Maybe a -> Maybe a -> Bool\n"
            "equalizer x y = True\n"
            "notTheSame :: Bool\n"
            "notTheSame = equalizer Nothing Nothing\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Ambiguous Type Var 1";
        const char* test_source = ""
            "maybeNothing :: Maybe c\n"
            "maybeNothing = Nothing\n"
            "const :: a -> b -> a\n"
            "const x y = x\n"
            "amb :: Int\n"
            "amb = const 22 Nothing\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_AMBIGUOUS_TYPE_VAR;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Ambiguous Type Var 2";
        const char* test_source = ""
            "class BS a where\n"
            "maybeNothing :: Maybe c\n"
            "maybeNothing = Nothing\n"
            "constBS :: (Num b, BS b) => a -> b -> a\n"
            "constBS x y = x\n"
            "amb :: Int\n"
            "amb = constBS 22 0\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_AMBIGUOUS_TYPE_VAR;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Polymorphic Recursive Value";
        const char* test_source = ""
            "polyNothing :: Maybe a\n"
            "polyNothing ~ Nothing = polyNothing\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_NON_CONCRETE_INITIALIZED_VALUE;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Initialized Non-Recursive Value";
        const char* test_source = ""
            "nonRec ~ True = True\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_NON_RECURSIVE_INITIALIZED_VALUE;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Mutual Recursion 2";
        const char* test_source = ""
            "rec1 :: a -> b -> b\n"
            "rec1 x y = rec2 True y\n"
            "rec2 :: a -> b -> b\n"
            "rec2 x y = rec1 x y\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Mutual Recursion 2";
        const char* test_source = ""
            "rec1 :: a -> b -> b\n"
            "rec1 x y = rec2 True y\n"
            "rec2 :: a -> b -> b\n"
            "rec2 x y = rec1 () y\n"
            "go :: Int\n"
            "go = rec1 (Just ()) 2\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Pattern Assignment 1";
        const char* test_source = ""
            "(left, right) = (1, 2.0)\n"
            "leftPlus :: Rational -> Rational\n"
            "leftPlus x = left\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Pattern Assignment 2";
        const char* test_source = ""
            "fst:: (a, b) -> a\n"
            "fst x = left where\n"
            "  (left, _) = x\n"
            "unitFirst :: ()\n"
            "unitFirst = fst ((), True)\n"
            "intFirst :: Int\n"
            "intFirst = fst (0, ())\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Pattern Assignment 2";
        const char* test_source = ""
            "leftRight = (left, right) where\n"
            "  (left, right) = (1, 2.0)\n"
            "intFloat :: (Int, Float)\n"
            "intFloat = leftRight\n"
            "rationalAudio :: (Rational, Audio)\n"
            "rationalAudio = leftRight\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Recursive Value 1";
        const char* test_source = ""
            "coolOsc :: Audio\n"
            "coolOsc ~ 0 = coolOsc + 1\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Recursive Value 2";
        const char* test_source = ""
            "coolOsc :: (Default a, Num a) => a\n"
            "coolOsc ~ default = coolOsc + 1\n"
            "coolInt :: Int\n"
            "coolInt = coolOsc\n"
            "coolAudio :: Audio\n"
            "coolAudio = coolOsc\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    // TODO: This should specialize dropOne should it not?
    {
        const char* test_name   = "Array is Ok 2";
        const char* test_source = ""
            "dropOne :: Array n a -> Array n a -> Array n a\n"
            "dropOne x _ = x\n"
            "arr1 = { 0, 1, 2, 3 }\n"
            "arr2 = { 3, 2, 1, 0 }\n"
            "one :: Array 4 Int\n"
            "one = dropOne arr1 arr2\n"
            "arr3 = { 0, 1, 2, 3, 4 }\n"
            "arr4 = { 4, 3, 2, 1, 0 }\n"
            "three :: Array 5 Int\n"
            "three = dropOne arr3 arr4\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    // TODO: Look into Tuples with initializers in parser
    // {
    //     const char* test_name   = "Recursive Pattern Assignment 1";
    //     const char* test_source = ""
    //         "(leftOsc ~ 0, _) = (leftOsc + 1, True)\n"
    //         "leftPlus :: Int\n"
    //         "leftPlus = leftOsc + 1\n";
    //     const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
    //     necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    // }

    // // TODO (Curtis 2-22-19): Pattern Assignment with Initializers with Numeric literals seems a little strange...
    // // TODO: check that default method is constant somehow...
    // {
    //     const char* test_name   = "Recursive Pattern Assignment 1";
    //     const char* test_source = ""
    //         "data Wrapper a = Wrapper a\n"
    //         "Wrapper (wrappedOsc ~ default) = Wrapper (wrappedOsc + 1)\n"
    //         "plusOne :: Audio\n"
    //         "plusOne = wrappedOsc + 1\n";
    //     const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
    //     necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    // }

    // NOTE: Multi-line Definitions are currently removed.
    // {
    //     const char* test_name   = "Multi-line Function";
    //     const char* test_source = ""
    //         "skateOrDie (Just _) = True\n"
    //         "skateOrDie _        = False\n"
    //         "die                 = skateOrDie (Just ())\n";
    //     const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
    //     necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    // }

}


/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef TYPE_CLASS_H
#define TYPE_CLASS_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "utility.h"
#include "hash_table.h"
#include "renamer.h"
#include "utility/list.h"
#include "type.h"

struct NecroTypeClass;
struct NecroInfer;
struct NecroBase;
struct NecroConstraintEnv;
struct NecroConstraint;
struct NecroConstraintList;

// TODO: Replace with NecroArenaList?
typedef struct NecroTypeClassMember
{
    NecroAstSymbol*              member_varid; // TODO: Rename
    struct NecroTypeClassMember* next;
} NecroTypeClassMember;

typedef struct NecroTypeClass
{
    NecroAst*                   ast;
    NecroType*                  type;
    NecroAstSymbol*             type_class_name;
    NecroAstSymbol*             type_var;
    NecroTypeClassMember*       members;
    size_t                      dependency_flag;
    NecroSymbol                 dictionary_name;
    struct NecroConstraintList* super_classes;
} NecroTypeClass;

typedef struct NecroDictionaryPrototype
{
    NecroAstSymbol*                  type_class_member_ast_symbol;
    NecroAstSymbol*                  instance_member_ast_symbol;
    struct NecroDictionaryPrototype* next;
} NecroDictionaryPrototype;

struct NecroTypeClassInstance;
NECRO_DECLARE_ARENA_LIST(struct NecroTypeClassInstance*, Instance, instance);
typedef struct NecroTypeClassInstance
{
    NecroAst*                 ast;
    NecroAstSymbol*           data_type_name;
    NecroAstSymbol*           type_class_name;
    NecroDictionaryPrototype* dictionary_prototype;
    NecroType*                data_type;
    NecroSymbol               dictionary_instance_name;
} NecroTypeClassInstance;

void                               necro_print_type_classes(struct NecroInfer* infer);

NecroResult(NecroType)             necro_create_type_class(struct NecroInfer* infer, NecroAst* type_class_ast);
NecroResult(NecroType)             necro_create_type_class_instance(struct NecroInfer* infer, NecroAst* instance_ast);

struct NecroConstraintList*        necro_constraint_list_union(NecroPagedArena* arena, struct NecroConstraintList* constraints1, struct NecroConstraintList* constraints2);
NecroResult(NecroConstraintList)   necro_constraint_list_from_ast(struct NecroInfer* infer, NecroAst* constraints_ast);
NecroResult(NecroType)             necro_constraint_ambiguous_type_class_check(NecroAstSymbol* type_sig_name, struct NecroConstraintList* constraints, NecroType* type);
NecroResult(NecroType)             necro_constraint_class_variable_check(NecroTypeClass* type_class, NecroAstSymbol* type_var, NecroAstSymbol* type_sig_symbol, struct NecroConstraintList* constraints);
void                               necro_constraint_list_apply(NecroPagedArena* arena, NecroType* type, struct NecroConstraintList* constraints);
NecroResult(NecroType)             necro_constraint_list_kinds_check(NecroPagedArena* arena, struct NecroBase* base, struct NecroConstraintList* constraints, NecroScope* scope);

bool                               necro_constraint_find_with_super_classes(struct NecroConstraintList* constraints_to_check, NecroType* original_object_type, struct NecroConstraint* constraint_to_find);
NecroResult(void)                  necro_constraint_simplify_class_constraint(NecroPagedArena* arena, struct NecroConstraintEnv* con_env, struct NecroBase* base, struct NecroConstraint* constraint);

#endif // TYPE_CLASS_H
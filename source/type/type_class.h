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

typedef NecroAst NecroAst;
struct  NecroTypeClass;
struct  NecroInfer;
struct  NecroBase;
struct NecroConstraintEnv;

// TODO: Move more of this shit into the .c file.

typedef struct NecroTypeClassContext
{
    NecroAstSymbol*               class_symbol; // TODO: Using this system this COULD be a TypeVar? Store NecroTypeClass in here?
    NecroAstSymbol*               var_symbol;
    struct NecroTypeClass*        type_class;
    struct NecroTypeClassContext* next;
} NecroTypeClassContext;

// TODO: Replace with NecroArenaList?
typedef struct NecroTypeClassMember
{
    NecroAstSymbol*              member_varid; // TODO: Rename
    struct NecroTypeClassMember* next;
} NecroTypeClassMember;

typedef struct NecroTypeClass
{
    NecroAst*              ast;
    NecroType*             type;
    NecroAstSymbol*        type_class_name;
    NecroAstSymbol*        type_var;
    NecroTypeClassMember*  members;
    NecroTypeClassContext* context;
    size_t                 dependency_flag;
    NecroSymbol            dictionary_name;
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
    NecroTypeClassContext*    context;
    NecroDictionaryPrototype* dictionary_prototype;
    NecroType*                data_type;
    NecroSymbol               dictionary_instance_name;
} NecroTypeClassInstance;

void                               necro_print_type_classes(struct NecroInfer* infer);

bool                               necro_context_and_super_classes_contain_class(NecroTypeClassContext* context, NecroTypeClassContext* type_class);
NecroTypeClassContext*             necro_union_contexts(NecroPagedArena* arena, NecroTypeClassContext* context1, NecroTypeClassContext* context2);
NecroResult(NecroType)             necro_ambiguous_type_class_check(NecroAstSymbol* type_sig_name, NecroTypeClassContext* context, NecroType* type);
NecroResult(NecroTypeClassContext) necro_ast_to_context(struct NecroInfer* infer, NecroAst* context_ast);
void                               necro_apply_constraints(NecroPagedArena* arena, NecroType* type, NecroTypeClassContext* context);
NecroTypeClassContext*             necro_create_type_class_context(NecroPagedArena* arena, NecroTypeClass* type_class, NecroAstSymbol* type_class_name, NecroAstSymbol* type_var, NecroTypeClassContext* next);
NecroResult(NecroType)             necro_propagate_type_classes(NecroPagedArena* arena, struct NecroConstraintEnv* con_env, struct NecroBase* base, NecroTypeClassContext* classes, NecroType* type, NecroScope* scope);

NecroResult(NecroType)             necro_create_type_class(struct NecroInfer* infer, NecroAst* type_class_ast);
NecroResult(NecroType)             necro_create_type_class_instance(struct NecroInfer* infer, NecroAst* instance_ast);

// TODO: Do we even need NecroTypeClassContext? Shouldn't we simply apply the constraints directly onto the TypeVar's themselves? Seems like we're confusing Syntax with Inference.

#endif // TYPE_CLASS_H
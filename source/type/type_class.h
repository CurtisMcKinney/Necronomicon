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

typedef NecroAST_Node_Reified NecroNode;
struct NecroTypeClass;

typedef struct NecroTypeClassContext
{
    NecroCon                      type_class_name;
    NecroCon                      type_var;
    struct NecroTypeClass*        type_class;
    struct NecroTypeClassContext* next;
} NecroTypeClassContext;

// TODO: Replace with NecroArenaList?
typedef struct NecroTypeClassMember
{
    NecroCon                     member_varid;
    struct NecroTypeClassMember* next;
} NecroTypeClassMember;

typedef struct NecroTypeClass
{
    NecroNode*             ast;
    NecroType*             type;
    NecroCon               type_class_name;
    NecroCon               type_var;
    NecroTypeClassMember*  members;
    NecroTypeClassContext* context;
    size_t                 dependency_flag;
    NecroSymbol            dictionary_name;
} NecroTypeClass;

typedef struct NecroDictionaryPrototype
{
    NecroCon                         type_class_member_varid;
    NecroCon                         prototype_varid;
    struct NecroDictionaryPrototype* next;
} NecroDictionaryPrototype;

struct NecroTypeClassInstance;
NECRO_DECLARE_ARENA_LIST(struct NecroTypeClassInstance*, Instance, instance);
typedef struct NecroTypeClassInstance
{
    NecroNode*                ast;
    NecroCon                  instance_name;
    NecroCon                  data_type_name;
    NecroCon                  type_class_name;
    NecroTypeClassContext*    context;
    NecroDictionaryPrototype* dictionary_prototype;
    NecroType*                data_type;
    NecroSymbol               dictionary_instance_name;
    NecroInstanceList*        super_instances;
} NecroTypeClassInstance;

typedef struct NecroMethodSub
{
    NecroVar   old_type_var;
    NecroType* new_type_var;
} NecroMethodSub;

NecroSymbol             necro_create_type_class_instance_name(NecroIntern* intern, NecroNode* ast);
void                    necro_print_type_classes(NecroInfer* infer);

bool                    necro_context_contains_class(NecroTypeClassContext* context, NecroTypeClassContext* type_class);
bool                    necro_context_and_super_classes_contain_class(NecroTypeClassContext* context, NecroTypeClassContext* type_class);
NecroTypeClassContext*  necro_union_contexts(NecroInfer* infer, NecroTypeClassContext* context1, NecroTypeClassContext* context2);
NecroTypeClassContext*  necro_union_contexts_to_same_var(NecroInfer* infer, NecroTypeClassContext* context1, NecroTypeClassContext* context2, NecroCon type_var);
bool                    necro_ambiguous_type_class_check(NecroInfer* infer, NecroSymbol type_sig_name, NecroTypeClassContext* context, NecroType* type);
NecroTypeClassContext*  necro_ast_to_context(NecroInfer* infer, NecroNode* context_ast);
void                    necro_apply_constraints(NecroInfer* infer, NecroType* type, NecroTypeClassContext* context);
NecroTypeClassContext*  necro_create_type_class_context(NecroPagedArena* arena, NecroTypeClass* type_class, NecroCon type_class_name, NecroCon type_var, NecroTypeClassContext* next);
NecroTypeClassContext*  necro_scrub_super_classes(NecroInfer* infer, NecroTypeClassContext* context);

// TODO: Replace with NecroArenaList???
typedef struct NecroTypeClassDictionaryContext
{
    NecroCon      type_class_name;
    NecroCon      type_var_name;
    NecroASTNode* dictionary_arg_ast;
    bool          intentially_not_included;
    struct NecroTypeClassDictionaryContext* next;
} NecroTypeClassDictionaryContext;
NecroTypeClassDictionaryContext* necro_create_type_class_dictionary_context(NecroPagedArena* arena, NecroCon type_class_name, NecroCon type_var_name, NecroASTNode* dictionary_arg_ast, NecroTypeClassDictionaryContext* next);
NECRO_RETURN_CODE                necro_type_class_translate(NecroInfer* infer, NecroNode* ast);
void                             necro_type_class_translate_go(NecroTypeClassDictionaryContext* dictionary_context, NecroInfer* infer, NecroNode* ast);
void                             necro_create_dictionary_data_declaration(NecroPagedArena* arena, NecroIntern* intern, NecroASTNode* type_class_ast);

NecroTypeClassInstance* necro_get_type_class_instance(NecroInfer* infer, NecroSymbol data_type_name, NecroSymbol type_class_name);
void                    necro_create_type_class(NecroInfer* infer, NecroNode* type_class_ast);
void                    necro_create_type_class_instance(NecroInfer* infer, NecroNode* instance_ast);

#endif // TYPE_CLASS_H
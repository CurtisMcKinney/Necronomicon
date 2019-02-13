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
struct NecroTypeClass;

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
    NecroInstanceList*        super_instances;
} NecroTypeClassInstance;

typedef struct NecroMethodSub
{
    NecroAstSymbol* old_type_var;
    NecroType*      new_type_var;
} NecroMethodSub;

void                               necro_print_type_classes(NecroInfer* infer);

bool                               necro_context_contains_class(NecroTypeClassContext* context, NecroTypeClassContext* type_class);
bool                               necro_context_and_super_classes_contain_class(NecroTypeClassContext* context, NecroTypeClassContext* type_class);
NecroTypeClassContext*             necro_union_contexts(NecroPagedArena* arena, NecroTypeClassContext* context1, NecroTypeClassContext* context2);
NecroTypeClassContext*             necro_union_contexts_to_same_var(NecroPagedArena* arena, NecroTypeClassContext* context1, NecroTypeClassContext* context2, NecroAstSymbol* var_symbol);
NecroResult(NecroType)             necro_ambiguous_type_class_check(NecroAstSymbol* type_sig_name, NecroTypeClassContext* context, NecroType* type);
NecroResult(NecroTypeClassContext) necro_ast_to_context(NecroInfer* infer, NecroAst* context_ast);
void                               necro_apply_constraints(NecroPagedArena* arena, NecroType* type, NecroTypeClassContext* context);
NecroTypeClassContext*             necro_create_type_class_context(NecroPagedArena* arena, NecroTypeClass* type_class, NecroAstSymbol* type_class_name, NecroAstSymbol* type_var, NecroTypeClassContext* next);
NecroTypeClassContext*             necro_scrub_super_classes(NecroTypeClassContext* context);

// TODO: Replace with NecroArenaList???
typedef struct NecroTypeClassDictionaryContext
{
    NecroAstSymbol* type_class_name;
    NecroAstSymbol* type_var_name;
    NecroAst*       dictionary_arg_ast;
    bool            intentially_not_included;
    struct NecroTypeClassDictionaryContext* next;
} NecroTypeClassDictionaryContext;
NecroTypeClassDictionaryContext* necro_create_type_class_dictionary_context(NecroPagedArena* arena, NecroAstSymbol* type_class_name, NecroAstSymbol* type_var_name, NecroAst* dictionary_arg_ast, NecroTypeClassDictionaryContext* next);
// NecroResult(void)                necro_type_class_translate(NecroInfer* infer, NecroAst* ast);
// NecroResult(NecroType)           necro_type_class_translate_go(NecroTypeClassDictionaryContext* dictionary_context, NecroInfer* infer, NecroAst* ast);
void                             necro_create_dictionary_data_declaration(NecroPagedArena* arena, NecroIntern* intern, NecroAst* type_class_ast);

NecroTypeClassInstance* necro_get_type_class_instance(NecroAstSymbol* data_type_symbol, NecroAstSymbol* type_class_symbol);
NecroResult(NecroType)  necro_create_type_class(NecroInfer* infer, NecroAst* type_class_ast);
NecroResult(NecroType)  necro_create_type_class_instance(NecroInfer* infer, NecroAst* instance_ast);

// TODO: Is This Cruft to be removed?
// NecroSymbol necro_create_type_class_instance_name(NecroIntern* intern, NecroAst* ast);

#endif // TYPE_CLASS_H
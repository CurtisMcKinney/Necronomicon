/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "codegen.h"
#include <stdarg.h>
#include <ctype.h>
#include <llvm-c/Analysis.h>
#include "type/infer.h"
#include "core/core.h"
#include "symtable.h"
#include "type/prim.h"
#include "intern.h"
#include "utility/small_array.h"

// things to reference:
//     * https://llvm.org/docs/LangRef.html#instruction-reference
//     * https://llvm.org/docs/tutorial/LangImpl04.html
//     * http://www.stephendiehl.com/llvm/
//     * https://lowlevelbits.org/how-to-use-llvm-api-with-swift.-addendum/

// TODO: Easy method to grab top level things with symbols
// TODO: LLVM License
// TODO: Core really should have source locations...
// TODO: Things that really need to happen:
//    - A Runtime with:
//        * Memory allocation onto the heap
//        * Value retrieval from the heap
//        * GC
//        * Continuous updates

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

extern DLLEXPORT uint64_t* _necro_alloc(uint32_t size)
{
    return malloc(size);
}

//=====================================================
// Create / Destroy
//=====================================================
NecroCodeGen necro_create_codegen(NecroInfer* infer, NecroIntern* intern, NecroSymTable* symtable, const char* module_name)
{
    // LLVMInitializeNativeTarget();
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef  mod     = LLVMModuleCreateWithNameInContext(module_name, context);

    LLVMTypeRef  necro_alloc_args[1] = { LLVMInt32TypeInContext(context) };
    LLVMValueRef necro_alloc         = LLVMAddFunction(mod, "_necro_alloc", LLVMFunctionType(LLVMPointerType(LLVMInt64TypeInContext(context), 0), necro_alloc_args, 1, false));
    LLVMSetLinkage(necro_alloc, LLVMExternalLinkage);
    LLVMSetFunctionCallConv(necro_alloc, LLVMFastCallConv);
    _necro_alloc(0);

    return (NecroCodeGen)
    {
        .arena          = necro_create_paged_arena(),
        .snapshot_arena = necro_create_snapshot_arena(),
        .infer          = infer,
        .intern         = intern,
        .symtable       = symtable,
        .context        = context,
        .mod            = mod,
        .builder        = LLVMCreateBuilderInContext(context),
        .target         = LLVMCreateTargetData(LLVMGetTarget(mod)),
        .necro_alloc    = necro_alloc,
        .error          = necro_create_error(),
        .current_block  = (NecroBlockIndex) { .index = 0 },
        .blocks         = 0,
        .is_top_level   = true
    };
}

void necro_destroy_codegen(NecroCodeGen* codegen)
{
    necro_destroy_paged_arena(&codegen->arena);
    necro_destroy_snapshot_arena(&codegen->snapshot_arena);
    LLVMContextDispose(codegen->context);
    LLVMDisposeBuilder(codegen->builder);
}

//=====================================================
// Utility
//=====================================================
size_t necro_expression_list_count(NecroCoreAST_Expression* list)
{
    if (list == NULL) return 0;
    assert(list->expr_type == NECRO_CORE_EXPR_LIST);
    size_t count = 0;
    while (list != NULL)
    {
        count++;
        list = list->list.next;
    }
    return count;
}

size_t necro_data_con_count(NecroCoreAST_DataCon* con)
{
    if (con == NULL) return 0;
    size_t count = 0;
    while (con != NULL)
    {
        count++;
        con = con->next;
    }
    return count;
}

inline LLVMTypeRef necro_get_data_type(NecroCodeGen* codegen)
{
    return necro_symtable_get(codegen->symtable, codegen->infer->prim_types->necro_data_type.id)->llvm_type;
}

inline LLVMTypeRef necro_get_value_ptr(NecroCodeGen* codegen)
{
    return LLVMPointerType(necro_symtable_get(codegen->symtable, codegen->infer->prim_types->necro_val_type.id)->llvm_type, 0);
}

LLVMValueRef necro_snapshot_gep(NecroCodeGen* codegen, const char* ptr_name, LLVMValueRef data_ptr, size_t num_indices, ...)
{
    va_list args;
    va_start(args, num_indices);
    uint32_t index = va_arg(args, uint32_t);
    LLVMValueRef* index_refs = necro_snapshot_arena_alloc(&codegen->snapshot_arena, sizeof(LLVMValueRef) * num_indices);
    for (size_t i = 0; i < num_indices; ++i)
    {
        index_refs[i] = LLVMConstInt(LLVMInt32TypeInContext(codegen->context), index, false);
        index         = va_arg(args, uint32_t);
    }
    va_end(args);
    return LLVMBuildGEP(codegen->builder, data_ptr, index_refs, num_indices, ptr_name);
}

// LLVMValueRef necro_snapshot_add_function(NecroCodeGen* codegen, const char* function_name, LLVMTypeRef return_type, size_t arg_count, ...)
LLVMValueRef necro_snapshot_add_function(NecroCodeGen* codegen, const char* function_name, LLVMTypeRef return_type, LLVMTypeRef* arg_refs, size_t arg_count)
{
    // va_list args;
    // va_start(args, arg_count);
    // LLVMTypeRef  arg      = va_arg(args, LLVMTypeRef);
    // LLVMTypeRef* arg_refs = (arg_count > 0) ? necro_snapshot_arena_alloc(&codegen->snapshot_arena, sizeof(LLVMValueRef) * arg_count) : NULL;
    // for (size_t i = 0; i < arg_count; ++i)
    // {
    //     arg_refs[i] = va_arg(args, LLVMTypeRef);
    // }
    // va_end(args);
    LLVMValueRef function = LLVMAddFunction(codegen->mod, function_name, LLVMFunctionType(return_type, arg_refs, arg_count, false));
    LLVMSetFunctionCallConv(function, LLVMFastCallConv);
    return function;
}

NECRO_DECLARE_SMALL_ARRAY(LLVMTypeRef, LLVMTypeRef, llvm_type_ref, 5);

size_t necro_codegen_count_num_args(NecroCodeGen* codegen, NecroCoreAST_DataCon* con)
{
    if (necro_is_codegen_error(codegen)) return 0;
    assert(codegen != NULL);
    assert(con != NULL);
    size_t count = 0;
    NecroCoreAST_Expression* args = con->arg_list;
    while (args != NULL)
    {
        assert(args->expr_type == NECRO_CORE_EXPR_LIST);
        count++;
        args = args->list.next;
    }
    return count;
}

char* necro_concat_strings(NecroSnapshotArena* arena,...)
{
    va_list args;
    va_start(args, arena);
    const char* arg = va_arg(args, const char*);
    size_t total_length = 1;
    while (arg != NULL)
    {
        total_length += strlen(arg);
        arg = va_arg(args, const char*);
    }
    va_end(args);
    char* buffer = necro_snapshot_arena_alloc(arena, total_length);
    buffer[0] = '\0';
    va_list args2;
    va_start(args2, arena);
    arg = va_arg(args2, const char*);
    while (arg != NULL)
    {
        strcat(buffer, arg);
        arg = va_arg(args2, const char*);
    }
    va_end(args2);
    return buffer;
}

void necro_init_necro_data(NecroCodeGen* codegen, LLVMValueRef data_ptr, uint32_t value_1, uint32_t value_2)
{
    NecroArenaSnapshot snapshot         = necro_get_arena_snapshot(&codegen->snapshot_arena);
    LLVMValueRef       necro_data_1     = necro_snapshot_gep(codegen, "necro_data_1", data_ptr, 3, 0, 0, 0);
    LLVMValueRef       necro_data_2     = necro_snapshot_gep(codegen, "necro_data_2", data_ptr, 3, 0, 0, 1);
    LLVMValueRef       const_value_1    = LLVMConstInt(LLVMInt32TypeInContext(codegen->context), value_1, false);
    LLVMValueRef       const_value_2    = LLVMConstInt(LLVMInt32TypeInContext(codegen->context), value_2, false);
    LLVMBuildStore(codegen->builder, const_value_1, necro_data_1);
    LLVMBuildStore(codegen->builder, const_value_2, necro_data_2);
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
}

LLVMValueRef necro_alloc_codegen(NecroCodeGen* codegen, uint64_t bytes)
{
    LLVMValueRef data_size     = LLVMConstInt(LLVMInt32TypeInContext(codegen->context), bytes, false);
    LLVMValueRef void_ptr      = LLVMBuildCall(codegen->builder, codegen->necro_alloc, &data_size, 1, "void_ptr");
    LLVMSetInstructionCallConv(void_ptr, LLVMFastCallConv);
    return void_ptr;
}

//=====================================================
// CodeGen
//=====================================================
LLVMValueRef necro_codegen_literal(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(ast->expr_type == NECRO_CORE_EXPR_LIT);
    switch (ast->lit.type)
    {
    case NECRO_AST_CONSTANT_INTEGER:
        LLVMConstInt(LLVMInt64TypeInContext(codegen->context), ast->lit.int_literal, true);
        break;
    case NECRO_AST_CONSTANT_FLOAT:
        LLVMConstReal(LLVMDoubleTypeInContext(codegen->context), ast->lit.double_literal);
        break;
    case NECRO_AST_CONSTANT_CHAR:
        LLVMConstInt(LLVMInt8TypeInContext(codegen->context), ast->lit.char_literal, true);
        break;
    }
    return NULL;
}

// TODO: I imagine this will be much more complicated once we take into account continuous update semantics
// Or maybe not? We're not lazy after all. Strict semantics means value will be precomputed...
// However, at the very least we're going to have to think about retrieving the value from the heap
// and GC.
LLVMValueRef necro_codegen_variable(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(ast->expr_type == NECRO_CORE_EXPR_VAR);
    LLVMValueRef value = necro_symtable_get(codegen->symtable, ast->var.id)->llvm_value;
    if (value == NULL)
    {
        assert(false && "Compiler bug: NULL value in necro_codegen_variable");
        return NULL;
    }
    return value;
}

// TODO: FINISH!
LLVMTypeRef necro_type_to_llvm_type(NecroCodeGen* codegen, NecroType* type)
{
    assert(type != NULL);
    type = necro_find(codegen->infer, type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:  return necro_symtable_get(codegen->symtable, codegen->infer->prim_types->necro_val_type.id)->llvm_type; // TODO: Is this right?!?!
    case NECRO_TYPE_APP:  assert(false); return NULL;
    case NECRO_TYPE_LIST: assert(false); return NULL;
    case NECRO_TYPE_FOR:  return NULL; // TODO: Finish
    case NECRO_TYPE_FUN:  return NULL; // TODO: Finish
    case NECRO_TYPE_CON:  return necro_symtable_get(codegen->symtable, type->con.con.id)->llvm_type;
    default: assert(false);
    }
    return NULL;
}

// TODO: FINISH!
LLVMTypeRef necro_get_ast_llvm_type(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    switch (ast->expr_type)
    {
    case NECRO_CORE_EXPR_LIT:
        switch (ast->lit.type)
        {
        case NECRO_AST_CONSTANT_INTEGER: return LLVMInt64TypeInContext(codegen->context);
        case NECRO_AST_CONSTANT_FLOAT:   return LLVMFloatTypeInContext(codegen->context);
        default:                         assert(false); return NULL;
        }
    case NECRO_CORE_EXPR_VAR:
    {
        NecroSymbolInfo* info = necro_symtable_get(codegen->symtable, ast->var.id);
        if (info->llvm_type == NULL)
            info->llvm_type = necro_type_to_llvm_type(codegen, info->type);
        assert(info->llvm_type != NULL);
        return info->llvm_type;
    }
    case NECRO_CORE_EXPR_BIND:
    {
        NecroSymbolInfo* info = necro_symtable_get(codegen->symtable, ast->bind.var.id);
        if (info->llvm_type == NULL)
            info->llvm_type = necro_type_to_llvm_type(codegen, info->type);
        assert(info->llvm_type != NULL);
        return info->llvm_type;
    }
    case NECRO_CORE_EXPR_DATA_CON:  return necro_symtable_get(codegen->symtable, ast->data_con.condid.id)->llvm_type;
    case NECRO_CORE_EXPR_APP:       assert(false); return NULL;
    case NECRO_CORE_EXPR_LAM:       assert(false); return NULL;
    case NECRO_CORE_EXPR_LET:       assert(false); return NULL;
    case NECRO_CORE_EXPR_CASE:      assert(false); return NULL;
    case NECRO_CORE_EXPR_TYPE:      assert(false); return NULL;
    case NECRO_CORE_EXPR_DATA_DECL: assert(false); return NULL;
    case NECRO_CORE_EXPR_LIST:      assert(false); return NULL; // used for top decls not language lists
    default:                        assert(false && "Unimplemented AST type in necro_codegen_go"); return NULL;
    }
}

//=====================================================
// DataCon
//=====================================================
void necro_codegen_data_con(NecroCodeGen* codegen, NecroCoreAST_DataCon* con, LLVMTypeRef data_type_ref, size_t max_arg_count, uint32_t con_number)
{
    if (necro_is_codegen_error(codegen)) return;
    NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&codegen->snapshot_arena);
    char*  con_name  = necro_concat_strings(&codegen->snapshot_arena, "_mk", necro_intern_get_string(codegen->intern, con->condid.symbol), NULL);
    size_t arg_count = necro_codegen_count_num_args(codegen, con);
    assert(arg_count <= max_arg_count);
    NecroLLVMTypeRefArray    args      = necro_create_llvm_type_ref_array(arg_count);
    NecroCoreAST_Expression* arg       = con->arg_list;
    for (size_t i = 0; i < arg_count; ++i)
    {
        assert(arg->expr_type == NECRO_CORE_EXPR_LIST);
        LLVMTypeRef arg_type = necro_get_ast_llvm_type(codegen, arg->list.expr);
        if (necro_is_codegen_error(codegen))
            goto necro_codegen_data_con_cleanup;
        *necro_llvm_type_ref_array_get(&args, i) = LLVMPointerType(arg_type, 0);
        arg = arg->list.next;
    }
    LLVMTypeRef       ret_type = LLVMFunctionType(LLVMPointerType(data_type_ref, 0), necro_llvm_type_ref_array_get(&args, 0), arg_count, 0);
    LLVMValueRef      make_con = LLVMAddFunction(codegen->mod, con_name, ret_type);
    LLVMSetFunctionCallConv(make_con, LLVMFastCallConv);
    LLVMBasicBlockRef entry    = LLVMAppendBasicBlock(make_con, "entry");
    LLVMPositionBuilderAtEnd(codegen->builder, entry);
    LLVMValueRef      void_ptr = necro_alloc_codegen(codegen, LLVMABISizeOfType(codegen->target, data_type_ref));
    LLVMValueRef      data_ptr = LLVMBuildBitCast(codegen->builder, void_ptr, LLVMPointerType(data_type_ref, 0), "data_ptr");
    necro_init_necro_data(codegen, data_ptr, 0, con_number);
    //--------------
    // Parameters
    for (size_t i = 0; i < max_arg_count; ++i)
    {
        char itoa_buff[6];
        char* location_name = necro_concat_strings(&codegen->snapshot_arena,"slot_", itoa(i, itoa_buff, 10), NULL);
        LLVMValueRef slot = necro_snapshot_gep(codegen, location_name, data_ptr, 2, 0, i + 1);
        if (i < arg_count)
        {
            char itoa_buff_2[6];
            char* value_name   = necro_concat_strings(&codegen->snapshot_arena,"param_", itoa(i, itoa_buff_2, 10), NULL);
            LLVMValueRef value = LLVMBuildBitCast(codegen->builder, LLVMGetParam(make_con, i), necro_get_value_ptr(codegen), value_name);
            LLVMBuildStore(codegen->builder, value, slot);
        }
        else
        {
            LLVMBuildStore(codegen->builder, LLVMConstPointerNull(necro_get_value_ptr(codegen)), slot);
        }
    }
    LLVMBuildRet(codegen->builder, data_ptr);
necro_codegen_data_con_cleanup:
    necro_destroy_llvm_type_ref_array(&args);
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
}

LLVMValueRef necro_codegen_data_declaration(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(ast->expr_type == NECRO_CORE_EXPR_DATA_DECL);

    size_t max_arg_count = 0;
    NecroCoreAST_DataCon* con = ast->data_decl.con_list;
    while (con != NULL)
    {
        size_t arg_count = necro_codegen_count_num_args(codegen, con);
        max_arg_count    = (arg_count > max_arg_count) ? arg_count : max_arg_count;
        con = con->next;
    }
    NecroLLVMTypeRefArray elems = necro_create_llvm_type_ref_array(max_arg_count + 1);
    *necro_llvm_type_ref_array_get(&elems, 0) = necro_get_data_type(codegen);
    for (size_t i = 1; i < max_arg_count + 1; ++i)
    {
        *necro_llvm_type_ref_array_get(&elems, i) = necro_get_value_ptr(codegen);
    }
    // Data Type
    LLVMTypeRef data_type_ref = LLVMStructCreateNamed(codegen->context, necro_intern_get_string(codegen->intern, ast->data_con.condid.symbol));
    LLVMStructSetBody(data_type_ref, necro_llvm_type_ref_array_get(&elems, 0), max_arg_count + 1, false);
    necro_symtable_get(codegen->symtable, ast->data_con.condid.id)->llvm_type = data_type_ref;

    con = ast->data_decl.con_list;
    size_t con_number = 0;
    while (con != NULL)
    {
        necro_codegen_data_con(codegen, con, data_type_ref, max_arg_count, con_number);
        con = con->next;
        con_number++;
    }

    // Cleanup
    necro_destroy_llvm_type_ref_array(&elems);
    return NULL;
}

//=====================================================
// Node
//=====================================================
// Lit doesn't define a Node, and doesn't Update
// (Let / Bind !?!?) without args defines a Node (struct, init_function, update_function), and introduces Node instance and updates it.
// (Let / Bind ?!?!!?) with args defines a Node (struct, init_function, update_function), but doesn't introduce a Node instance, and doesn't Update.
// Var with no args doesn't define a Node but queries the state of a Node instance or is applied as a function
// App doesn't define a Node, introduces a node instance and updates it.
// Lam at the begining of a Bind defines a function / Node. Doesn't introduce a Node type or update function, and doesn't introduce a node instance or update it.
// Lam as a value is part of the mess related to Higher-order functions/closures. Left for future Curtis to figure out.
// Case doesn't introduce an Node or Update, but doesn't generate stateless code.

struct NecroNodePrototype;
void necro_calculate_node_prototype(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, struct NecroNodePrototype* outer_node);
typedef struct NecroNodeInstance
{
    struct NecroNodePrototype* prototype;
    NecroVar                   bind_var;
    NecroSymbol                name_in_outer_mk_function;
} NecroNodeInstance;

NECRO_DECLARE_ARENA_LIST(LLVMTypeRef, LLVMType, llvm_type);
NECRO_DECLARE_ARENA_LIST(NecroNodeInstance, NodeInstance, node_instance);
NECRO_DECLARE_ARENA_LIST(NecroCon, Con, necro_con);
typedef enum
{
    NECRO_NODE_CONSTANT,
    NECRO_NODE_STATELESS,
    NECRO_NODE_STATEFUL
} NECRO_NODE_TYPE;

// Pass function values as nodes or mk_node functions, or !?
// Captured variables are captured via nodes, with their mk_node or init_node function
typedef struct NecroNodePrototype
{
    NecroSymbol            name;
    LLVMTypeRef            node_type;          // Type of the actual node
    LLVMTypeRef            node_value_type;    // The value type that the node calculates
    NecroNodeInstanceList* args;               // Arguments to be passed in. Will be passed in as raw values in registers or on the stack
    NecroNodeInstanceList* owned_instances;    // Owned Node instances which are updated with this node
    NecroNodeInstanceList* captured_variables; // Captured variables which are queried for their current value, not updated
    NecroNodeInstanceList* init_args;          // Need way to calculate this, and to union to different lists
    LLVMValueRef           mk_function;
    LLVMValueRef           init_function;
    bool                   init_args_calculated;
    bool                   is_function;        // Is this necessary?!
    NECRO_NODE_TYPE        type;
} NecroNodePrototype;

NecroNodePrototype* necro_create_necro_node_prototype(NecroCodeGen* codegen, const char* name, LLVMTypeRef node_type, LLVMTypeRef node_value_type, NecroNodeInstanceList* args, NecroNodeInstanceList* owned_instances, NecroNodeInstanceList* captured_variables, NECRO_NODE_TYPE type)
{
    NecroNodePrototype* node_prototype   = necro_paged_arena_alloc(&codegen->arena, sizeof(NecroNodePrototype));
    node_prototype->name                 = necro_intern_string(codegen->intern, name);
    node_prototype->node_type            = node_type;
    node_prototype->node_value_type      = node_value_type;
    node_prototype->args                 = args;
    node_prototype->owned_instances      = owned_instances;
    node_prototype->captured_variables   = captured_variables;
    node_prototype->init_args            = NULL;
    node_prototype->mk_function          = NULL;
    node_prototype->init_function        = NULL;
    node_prototype->init_args_calculated = false;
    node_prototype->type                 = type;
    return node_prototype;
}

// Takes a list of captured variables as arguments to make functions, along with pointer to memory!
LLVMValueRef necro_codegen_mk_node(NecroCodeGen* codegen, NecroNodePrototype* node)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(node != NULL);

    NecroArenaSnapshot snapshot  = necro_get_arena_snapshot(&codegen->snapshot_arena);
    char*              mk_name   = necro_concat_strings(&codegen->snapshot_arena, "_mk", necro_intern_get_string(codegen->intern, node->name), NULL);
    LLVMValueRef       mk_alloc  = necro_snapshot_add_function(codegen, mk_name, LLVMPointerType(node->node_type, 0), NULL, 0);
    LLVMBasicBlockRef  entry     = LLVMAppendBasicBlock(mk_alloc, "entry");
    LLVMPositionBuilderAtEnd(codegen->builder, entry);
    LLVMValueRef       void_ptr  = necro_alloc_codegen(codegen, LLVMABISizeOfType(codegen->target, node->node_type));
    LLVMValueRef       node_ptr  = LLVMBuildBitCast(codegen->builder, void_ptr, LLVMPointerType(node->node_type, 0), "node_ptr");
    necro_init_necro_data(codegen, node_ptr, 0, 0);
    LLVMValueRef       value_ptr = necro_snapshot_gep(codegen, "value_ptr", node_ptr, 2, 0, 1);
    LLVMBuildStore(codegen->builder, LLVMConstPointerNull(LLVMPointerType(node->node_value_type, 0)), value_ptr);

    //--------------
    // NULL out Owned Instance pointers
    NecroNodeInstanceList* owned_instances = node->owned_instances;
    size_t i = 2;
    while (owned_instances != NULL)
    {
        LLVMValueRef node = necro_snapshot_gep(codegen, "owned", node_ptr, 2, 0, i);
        LLVMBuildStore(codegen->builder, LLVMConstPointerNull(LLVMPointerType(owned_instances->data.prototype->node_type, 0)), node);
        i++;
        owned_instances = owned_instances->next;
    }

    //--------------
    // NULL out Captured Variable pointers
    NecroNodeInstanceList* captured_variables = node->captured_variables;
    while (captured_variables != NULL)
    {
        LLVMValueRef node = necro_snapshot_gep(codegen, "captured", node_ptr, 2, 0, i);
        LLVMBuildStore(codegen->builder, LLVMConstPointerNull(LLVMPointerType(captured_variables->data.prototype->node_type, 0)), node);
        i++;
        captured_variables = captured_variables->next;
    }

    LLVMBuildRet(codegen->builder, node_ptr);
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
    return mk_alloc;
}

// TODO: delay primitive!
// Only call init at the beginning of the first block that the node will be used in!
// This preserves the ability to do self and mutual recursion without going infinite!
// If we inline all owned node instances perhaps we can obviate the need for captured variables in values (but not functions)
// That leaves how we handle function closures / Higher order functions, which is a whole thing to be solved.
// Init function needs to be passed
//     1. All captured variable nodes
//     2. All captured variable nodes for each owned node (which will get forwarded to each owned node when they are initialized)
LLVMValueRef necro_codegen_init_node(NecroCodeGen* codegen, NecroNodePrototype* node)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(node != NULL);

    NecroArenaSnapshot snapshot  = necro_get_arena_snapshot(&codegen->snapshot_arena);
    char*              init_name = necro_concat_strings(&codegen->snapshot_arena, "_init", necro_intern_get_string(codegen->intern, node->name), NULL);

    // Calculate init_node args

    assert(node->node_type != NULL);
    LLVMTypeRef        args[1]   = { LLVMPointerType(node->node_type, 0) };
    LLVMValueRef       init_fn   = necro_snapshot_add_function(codegen, init_name, LLVMVoidTypeInContext(codegen->context), args, 1);
    LLVMBasicBlockRef  entry     = LLVMAppendBasicBlock(init_fn, "entry");
    LLVMPositionBuilderAtEnd(codegen->builder, entry);

    //--------------
    // alloc each owned node with appropriate mk_node function
    NecroNodeInstanceList* owned_instances = node->owned_instances;
    size_t i = 2;
    while (owned_instances != NULL)
    {
        LLVMValueRef owned_data = LLVMBuildCall(codegen->builder, owned_instances->data.prototype->mk_function, NULL, 0, "owned_data");
        LLVMSetInstructionCallConv(owned_data, LLVMFastCallConv);
        LLVMValueRef owned_ptr  = necro_snapshot_gep(codegen, "owned_ptr", LLVMGetParam(init_fn, 0), 2, 0, i);
        LLVMBuildStore(codegen->builder, owned_data, owned_ptr);
        i++;
        owned_instances = owned_instances->next;
    }

    LLVMBuildRetVoid(codegen->builder);
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
    return init_fn;
}

NecroNodePrototype* necro_declare_node_prototype(NecroCodeGen* codegen, NecroVar bind_var)
{
    NecroNodePrototype* prototype = necro_symtable_get(codegen->symtable, bind_var.id)->node_prototype;
    if (prototype != NULL)
        return prototype;
    char  buf2[10];
    char* node_name = necro_concat_strings(&codegen->snapshot_arena, necro_intern_get_string(codegen->intern, bind_var.symbol), "_", itoa(bind_var.id.id, buf2, 10), "_Node", NULL);
    node_name[0]    = toupper(node_name[0]);
    LLVMTypeRef         node_type_ref  = LLVMStructCreateNamed(codegen->context, node_name);
    LLVMTypeRef         value_type     = necro_type_to_llvm_type(codegen, necro_symtable_get(codegen->symtable, bind_var.id)->type);
    assert(value_type != NULL);
    NecroNodePrototype* node_prototype = necro_create_necro_node_prototype(codegen, node_name, node_type_ref, value_type, NULL, NULL, NULL, NECRO_NODE_STATEFUL);
    necro_symtable_get(codegen->symtable, bind_var.id)->node_prototype = node_prototype;
    return node_prototype;
}

void necro_calculate_node_prototype_bind(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer_node)
{
    if (necro_is_codegen_error(codegen)) return;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_BIND);
    NecroArenaSnapshot  snapshot       = necro_get_arena_snapshot(&codegen->snapshot_arena);
    NecroNodePrototype* node_prototype = necro_declare_node_prototype(codegen, ast->bind.var);
    // TODO: Better way to distinguish function from value!
    if (ast->bind.expr->expr_type != NECRO_CORE_EXPR_LAM)
    {
        if (outer_node == NULL)
        {
            //--------------------
            // Global
            // Create global instance
            char buf2[10];
            const char* bind_name = necro_concat_strings(&codegen->snapshot_arena, necro_intern_get_string(codegen->intern, ast->bind.var.symbol), "_", itoa(ast->bind.var.id.id, buf2, 10), NULL);
            LLVMTypeRef bind_type = necro_get_ast_llvm_type(codegen, ast);
            assert(bind_type != NULL);
            LLVMValueRef bind_value = LLVMAddGlobal(codegen->mod, LLVMPointerType(node_prototype->node_type, 0), bind_name);
        }
        else
        {
            //--------------------
            // Local
            // Add instance to outer_node
            NecroNodeInstance node_instance = (NecroNodeInstance) { .prototype = node_prototype, .bind_var = ast->bind.var };
            outer_node->owned_instances = necro_cons_node_instance_list(&codegen->arena, node_instance, outer_node->owned_instances);
        }

        // Go Deeper
        necro_calculate_node_prototype(codegen, ast->bind.expr, node_prototype);

        //--------------------
        // Create Node body
        node_prototype->owned_instances     = necro_reverse_node_instance_list(&codegen->arena, node_prototype->owned_instances);    // We consed onto list, time to reverse
        node_prototype->captured_variables  = necro_reverse_node_instance_list(&codegen->arena, node_prototype->captured_variables); // We consed onto list, time to reverse
        size_t       num_node_instances     = necro_count_node_instance_list(node_prototype->owned_instances);
        size_t       num_captured_variables = necro_count_node_instance_list(node_prototype->captured_variables);
        size_t       num_elems              = 2 + num_node_instances + num_captured_variables;
        LLVMTypeRef* node_elems             = necro_snapshot_arena_alloc(&codegen->snapshot_arena, sizeof(LLVMTypeRef) * num_elems);
        node_elems[0]                       = necro_get_data_type(codegen);
        node_elems[1]                       = LLVMPointerType(node_prototype->node_value_type, 0);
        size_t elem = 2;
        NecroNodeInstanceList* owned_instances = node_prototype->owned_instances;
        while (owned_instances != NULL)
        {
            owned_instances->data.prototype = necro_declare_node_prototype(codegen, owned_instances->data.bind_var);
            node_elems[elem] = LLVMPointerType(owned_instances->data.prototype->node_type, 0);
            elem++;
            owned_instances  = owned_instances->next;
        }
        NecroNodeInstanceList* captured_variables = node_prototype->captured_variables;
        while (captured_variables != NULL)
        {
            captured_variables->data.prototype = necro_declare_node_prototype(codegen, captured_variables->data.bind_var);
            node_elems[elem] = LLVMPointerType(captured_variables->data.prototype->node_type, 0);
            elem++;
            captured_variables  = captured_variables->next;
        }
        LLVMStructSetBody(node_prototype->node_type, node_elems, num_elems, false);

        // TODO: Finish!
        //--------------------
        // Create Node functions
        node_prototype->mk_function   = necro_codegen_mk_node(codegen, node_prototype);
        node_prototype->init_function = necro_codegen_init_node(codegen, node_prototype);
        // necro_codegen_update_node(codegen, node_prototype);
    }
    else
    {
        // Function binding
        // node_prototype = necro_create_necro_node_prototype(codegen, node_type_ref, value_type, NULL, NULL, NECRO_NODE_STATEFUL);
        // necro_symtable_get(codegen->symtable, ast->bind.var.id)->node_prototype = node_prototype;
        // necro_calculate_node_prototype(codegen, ast->bind.expr, node_prototype);
        // Get args
        // calculate body nodes
        // Create Node body
        // Create mk_function
        // Create init_function
        // Create update_function
    }

    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
}

void necro_calculate_node_prototype_let(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer_node)
{
    if (necro_is_codegen_error(codegen)) return;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_LET);
    assert(ast->let.bind != NULL);
    assert(ast->let.bind->expr_type == NECRO_CORE_EXPR_BIND);
    necro_calculate_node_prototype_bind(codegen, ast->let.bind, outer_node);
    necro_calculate_node_prototype(codegen, ast->let.expr, outer_node);
}

void necro_calculate_node_prototype_app(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer_node)
{
    if (necro_is_codegen_error(codegen)) return;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_APP);
    assert(ast->app.exprA != NULL);
    assert(ast->app.exprB != NULL);
    // TODO: FINISH!
    // TODO/HACK: Lame temp solution, Find actual solution!
    if (ast->app.exprA->expr_type != NECRO_CORE_EXPR_VAR)
        necro_calculate_node_prototype(codegen, ast->app.exprA, outer_node);
    necro_calculate_node_prototype(codegen, ast->app.exprB, outer_node);
}

void necro_calculate_node_prototype_var(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer_node)
{
    if (necro_is_codegen_error(codegen)) return;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_VAR);
    //----------
    // If it's already an argument or a captured variable, ignore it.
    NecroNodeInstanceList* args = outer_node->args;
    while (args != NULL)
    {
        if (ast->var.id.id == args->data.bind_var.id.id)
            return;
        args = args->next;
    }
    NecroNodeInstanceList* owned_instances = outer_node->owned_instances;
    while (owned_instances != NULL)
    {
        if (ast->var.id.id == owned_instances->data.bind_var.id.id)
            return;
        owned_instances = owned_instances->next;
    }
    //----------
    // Otherwise add it to the captured variable list
    NecroNodeInstance node_instance = (NecroNodeInstance)
    {
        .prototype = necro_symtable_get(codegen->symtable, ast->var.id)->node_prototype,
        .bind_var  = (NecroVar) { .id = ast->var.id, .symbol = ast->var.symbol },
    };
    outer_node->captured_variables = necro_cons_node_instance_list(&codegen->arena, node_instance, outer_node->captured_variables);
}

void necro_calculate_node_prototype_lam(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer_node)
{
    if (necro_is_codegen_error(codegen)) return;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_LAM);
}

void necro_calculate_node_prototype_case(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer_node)
{
    if (necro_is_codegen_error(codegen)) return;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_CASE);
}

// Perhaps do full on ast crawl and store these into symtable.
// Then we can reference them whenever we want!
void necro_calculate_node_prototype(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer_node)
{
    if (necro_is_codegen_error(codegen)) return;
    assert(codegen != NULL);
    assert(ast != NULL);
    NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&codegen->snapshot_arena);
    switch (ast->expr_type)
    {
    case NECRO_CORE_EXPR_LET:       necro_calculate_node_prototype_let(codegen, ast, outer_node);  return;
    case NECRO_CORE_EXPR_BIND:      necro_calculate_node_prototype_bind(codegen, ast, outer_node); return;
    case NECRO_CORE_EXPR_APP:       necro_calculate_node_prototype_app(codegen, ast, outer_node);  return;
    case NECRO_CORE_EXPR_VAR:       necro_calculate_node_prototype_var(codegen, ast, outer_node);  return;
    case NECRO_CORE_EXPR_LAM:       necro_calculate_node_prototype_lam(codegen, ast, outer_node);  return;
    case NECRO_CORE_EXPR_CASE:      necro_calculate_node_prototype_case(codegen, ast, outer_node); return;
    case NECRO_CORE_EXPR_LIT:       return; // No node behavior
    // Should never happen
    case NECRO_CORE_EXPR_DATA_DECL: assert(false && "Should never reach DATA_DECL case in necro_calculate_node_prototype"); return;
    case NECRO_CORE_EXPR_DATA_CON:  assert(false && "Should never reach DATA_CON  case in necro_calculate_node_prototype"); return;
    case NECRO_CORE_EXPR_TYPE:      assert(false && "Should never reach EXPR_TYPE case in necro_calculate_node_prototype"); return;
    case NECRO_CORE_EXPR_LIST:      assert(false && "Should never reach EXPR_LIST case in necro_calculate_node_prototype"); return; // used for top decls not language lists
    default:                        assert(false && "Unimplemented AST type in necro_calculate_node_prototype"); return;
    }
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
}

LLVMValueRef necro_codegen_bind(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    necro_calculate_node_prototype(codegen, ast, NULL);
    return NULL;
}

//=====================================================
// Go
//=====================================================
LLVMValueRef necro_codegen_go(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(codegen != NULL);
    assert(ast != NULL);
    NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&codegen->snapshot_arena);
    switch (ast->expr_type)
    {
    case NECRO_CORE_EXPR_LIT:       return necro_codegen_literal(codegen, ast);
    case NECRO_CORE_EXPR_VAR:       return necro_codegen_variable(codegen, ast);
    case NECRO_CORE_EXPR_BIND:      return necro_codegen_bind(codegen, ast);
    case NECRO_CORE_EXPR_APP:       return NULL;
    case NECRO_CORE_EXPR_LAM:       return NULL;
    case NECRO_CORE_EXPR_LET:       return NULL;
    case NECRO_CORE_EXPR_CASE:      return NULL;
    case NECRO_CORE_EXPR_DATA_DECL: return necro_codegen_data_declaration(codegen, ast);
    case NECRO_CORE_EXPR_DATA_CON:  return NULL;
    case NECRO_CORE_EXPR_TYPE:      assert(false); return NULL;
    case NECRO_CORE_EXPR_LIST:      assert(false); return NULL; // used for top decls not language lists
    default:                        assert(false && "Unimplemented AST type in necro_codegen_go"); return NULL;
    }
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
    return NULL;
}

NECRO_RETURN_CODE necro_verify_and_dump_codegen(NecroCodeGen* codegen)
{
    char* error = NULL;
    LLVMVerifyModule(codegen->mod, LLVMAbortProcessAction, &error);
    if (strlen(error) != 0)
    {
        printf("LLVM error message: %s\n", error);
        LLVMDisposeMessage(error);
        return NECRO_ERROR;
    }
    LLVMDisposeMessage(error);
    const char* module_string_rep = LLVMPrintModuleToString(codegen->mod);
    printf("\n%s\n", module_string_rep);

    // LLVMDisposeBuilder(codegen->builder);
    return NECRO_SUCCESS;
}

NECRO_RETURN_CODE necro_codegen(NecroCodeGen* codegen, NecroCoreAST* core_ast)
{
    assert(codegen != NULL);
    assert(core_ast != NULL);
    assert(core_ast->root->expr_type == NECRO_CORE_EXPR_LIST);
    if (necro_codegen_primitives(codegen) == NECRO_ERROR)
        return codegen->error.return_code;
    NecroCoreAST_Expression* top_level_list = core_ast->root;
    while (top_level_list != NULL)
    {
        if (top_level_list->list.expr != NULL)
            necro_codegen_go(codegen, top_level_list->list.expr);
        if (necro_is_codegen_error(codegen)) return codegen->error.return_code;
        top_level_list = top_level_list->list.next;
    }
   if (necro_verify_and_dump_codegen(codegen) == NECRO_ERROR)
        return NECRO_ERROR;
    return NECRO_SUCCESS;
}
/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "codegen_llvm.h"
#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Transforms/Scalar.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassManagerBuilder.h>
#include "symtable.h"

///////////////////////////////////////////////////////
// Create / Destroy
///////////////////////////////////////////////////////
NecroCodeGenLLVM necro_create_codegen_llvm(NecroIntern* intern, NecroSymTable* symtable, NecroPrimTypes* prim_types)
{
    // LLVMLinkInMCJIT();
    LLVMInitializeNativeTarget();
    // LLVMInitializeNativeAsmPrinter();
    // LLVMInitializeNativeAsmParser();
    LLVMContextRef     context          = LLVMContextCreate();
    LLVMModuleRef      mod              = LLVMModuleCreateWithNameInContext("necro", context);
    LLVMTargetDataRef  target           = LLVMCreateTargetData(LLVMGetTarget(mod));
    LLVMPassManagerRef fn_pass_manager  = LLVMCreateFunctionPassManagerForModule(mod);
    LLVMPassManagerRef mod_pass_manager = LLVMCreatePassManager();
    // LLVMAddCFGSimplificationPass(fn_pass_manager);
    // LLVMAddDeadStoreEliminationPass(fn_pass_manager);
    // LLVMAddTailCallEliminationPass(fn_pass_manager);
    // LLVMAddInstructionCombiningPass(fn_pass_manager);
    // LLVMInitializeFunctionPassManager(fn_pass_manager);
    // LLVMFinalizeFunctionPassManager(fn_pass_manager);
    // LLVMPassManagerBuilderRef pass_manager_builder = LLVMPassManagerBuilderCreate();
    // LLVMPassManagerBuilderSetOptLevel(pass_manager_builder, 0);
    // LLVMPassManagerBuilderUseInlinerWithThreshold(pass_manager_builder, 2);
    // LLVMPassManagerBuilderPopulateModulePassManager(pass_manager_builder, mod_pass_manager);
    return (NecroCodeGenLLVM)
    {
        .arena            = necro_create_paged_arena(),
        .snapshot_arena   = necro_create_snapshot_arena(),
        .intern           = intern,
        .symtable         = symtable,
        .prim_types       = prim_types,
        .codegen_symtable = necro_create_necro_codegen_symbol_vector(),
        .context          = context,
        .builder          = LLVMCreateBuilderInContext(context),
        .mod              = mod,
        .target           = target,
        .fn_pass_manager  = fn_pass_manager,
        .mod_pass_manager = mod_pass_manager,
    };
}

void necro_destroy_codegen_llvm(NecroCodeGenLLVM* codegen)
{
    assert(codegen != NULL);
    LLVMContextDispose(codegen->context);
    LLVMDisposeBuilder(codegen->builder);
    necro_destroy_paged_arena(&codegen->arena);
    necro_destroy_snapshot_arena(&codegen->snapshot_arena);
}

///////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////
LLVMTypeRef  necro_machine_type_to_llvm_type(NecroCodeGenLLVM* codegen, NecroMachineType* machine_type);
void         necro_codegen_function(NecroCodeGenLLVM* codegen, NecroMachineAST* ast);
LLVMValueRef necro_codegen_block_statement(NecroCodeGenLLVM* codegen, NecroMachineAST* ast);
LLVMValueRef necro_codegen_value(NecroCodeGenLLVM* codegen, NecroMachineAST* ast);
LLVMValueRef necro_codegen_terminator(NecroCodeGenLLVM* codegen, NecroTerminator* term);

///////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////
NECRO_RETURN_CODE necro_verify_and_dump_codegen_llvm(NecroCodeGenLLVM* codegen)
{
    LLVMRunPassManager(codegen->mod_pass_manager, codegen->mod);
    fflush(stdout);
    fflush(stderr);
    LLVMDumpModule(codegen->mod);
    char* error = NULL;
    LLVMVerifyModule(codegen->mod, LLVMAbortProcessAction, &error);
    if (strlen(error) != 0)
    {
        LLVMDumpModule(codegen->mod);
        fprintf(stderr, "LLVM error: %s\n", error);
        LLVMDisposeMessage(error);
        return NECRO_ERROR;
    }
    LLVMDisposeMessage(error);
    return NECRO_SUCCESS;
}

NecroCodeGenSymbolInfo* necro_codegen_symtable_get(NecroCodeGenLLVM* codegen, NecroVar var)
{
    assert(codegen != NULL);
    assert(var.id.id != 0);
    while (var.id.id >= codegen->codegen_symtable.length)
    {
        NecroCodeGenSymbolInfo info = { NULL, NULL, NULL };
        necro_push_necro_codegen_symbol_vector(&codegen->codegen_symtable, &info);
    }
    return codegen->codegen_symtable.data + var.id.id;
}

LLVMTypeRef necro_machine_type_to_llvm_type(NecroCodeGenLLVM* codegen, NecroMachineType* machine_type)
{
    assert(machine_type != NULL);
    switch (machine_type->type)
    {
    case NECRO_MACHINE_TYPE_UINT1:  return LLVMInt1TypeInContext(codegen->context);
    case NECRO_MACHINE_TYPE_UINT8:  return LLVMInt8TypeInContext(codegen->context);
    case NECRO_MACHINE_TYPE_UINT16: return LLVMInt16TypeInContext(codegen->context);
    case NECRO_MACHINE_TYPE_UINT32: return LLVMInt32TypeInContext(codegen->context);
    case NECRO_MACHINE_TYPE_UINT64: return LLVMInt64TypeInContext(codegen->context);
    case NECRO_MACHINE_TYPE_INT64:  return LLVMInt64TypeInContext(codegen->context);
    case NECRO_MACHINE_TYPE_F64:    return LLVMDoubleTypeInContext(codegen->context);
    case NECRO_MACHINE_TYPE_CHAR:   assert(false); return NULL;
    case NECRO_MACHINE_TYPE_VOID:   return LLVMVoidTypeInContext(codegen->context);
    case NECRO_MACHINE_TYPE_PTR:    return LLVMPointerType(necro_machine_type_to_llvm_type(codegen, machine_type->ptr_type.element_type), 0);
    case NECRO_MACHINE_TYPE_STRUCT:
    {
        NecroCodeGenSymbolInfo* info = necro_codegen_symtable_get(codegen, machine_type->struct_type.name);
        if (info->type == NULL)
        {
            LLVMTypeRef struct_type = LLVMStructCreateNamed(codegen->context, necro_intern_get_string(codegen->intern, machine_type->struct_type.name.symbol));
            info->type = struct_type;
            LLVMTypeRef* elements = necro_paged_arena_alloc(&codegen->arena, machine_type->struct_type.num_members * sizeof(LLVMTypeRef));
            for (size_t i = 0; i < machine_type->struct_type.num_members; ++i)
            {
                elements[i] = necro_machine_type_to_llvm_type(codegen, machine_type->struct_type.members[i]);
            }
            LLVMStructSetBody(struct_type, elements, machine_type->struct_type.num_members, false);
        }
        return info->type;
    }
    case NECRO_MACHINE_TYPE_FN:
    {
        LLVMTypeRef  return_type = necro_machine_type_to_llvm_type(codegen, machine_type->fn_type.return_type);
        LLVMTypeRef* parameters  = necro_paged_arena_alloc(&codegen->arena, machine_type->fn_type.num_parameters * sizeof(LLVMTypeRef));
        for (size_t i = 0; i < machine_type->fn_type.num_parameters; ++i)
        {
            parameters[i] = necro_machine_type_to_llvm_type(codegen, machine_type->fn_type.parameters[i]);
        }
        return LLVMFunctionType(return_type, parameters, machine_type->fn_type.num_parameters, false);
    }
    default:
        assert(false);
        return NULL;
    }
}

LLVMValueRef necro_const_zero_machine_type(NecroCodeGenLLVM* codegen, NecroMachineType* machine_type)
{
    assert(machine_type != NULL);
    switch (machine_type->type)
    {
    case NECRO_MACHINE_TYPE_UINT1:  return LLVMConstInt(LLVMInt1TypeInContext(codegen->context), 0, false);
    case NECRO_MACHINE_TYPE_UINT8:  return LLVMConstInt(LLVMInt8TypeInContext(codegen->context), 0, false);
    case NECRO_MACHINE_TYPE_UINT16: return LLVMConstInt(LLVMInt16TypeInContext(codegen->context), 0, false);
    case NECRO_MACHINE_TYPE_UINT32: return LLVMConstInt(LLVMInt32TypeInContext(codegen->context), 0, false);
    case NECRO_MACHINE_TYPE_UINT64: return LLVMConstInt(LLVMInt64TypeInContext(codegen->context), 0, false);
    case NECRO_MACHINE_TYPE_INT64:  return LLVMConstInt(LLVMInt64TypeInContext(codegen->context), 0, false);
    case NECRO_MACHINE_TYPE_F64:    return LLVMConstReal(LLVMDoubleTypeInContext(codegen->context), 0.f);
    case NECRO_MACHINE_TYPE_CHAR:   assert(false); return NULL;
    case NECRO_MACHINE_TYPE_VOID:   assert(false); return NULL;
    case NECRO_MACHINE_TYPE_PTR:    return LLVMConstPointerNull(LLVMPointerType(necro_machine_type_to_llvm_type(codegen, machine_type->ptr_type.element_type), 0));
    case NECRO_MACHINE_TYPE_STRUCT:
    {
        LLVMTypeRef   struct_type = necro_codegen_symtable_get(codegen, machine_type->struct_type.name)->type;
        LLVMValueRef* elements    = necro_paged_arena_alloc(&codegen->arena, machine_type->struct_type.num_members * sizeof(LLVMValueRef));
        for (size_t i = 0; i < machine_type->struct_type.num_members; ++i)
        {
            elements[i] = necro_const_zero_machine_type(codegen, machine_type->struct_type.members[i]);
        }
        return LLVMConstNamedStruct(struct_type, elements, machine_type->struct_type.num_members);
    }
    case NECRO_MACHINE_TYPE_FN:
        assert(false);
        return NULL;
    default:
        assert(false);
        return NULL;
    }
}

LLVMValueRef necro_maybe_cast(NecroCodeGenLLVM* codegen, LLVMValueRef value, LLVMTypeRef type_to_match)
{
    LLVMTypeRef value_type = LLVMTypeOf(value);
    if (value_type != type_to_match)
    {
        if (LLVMGetElementType(value_type) != codegen->poly_type && LLVMGetElementType(type_to_match) != codegen->poly_type)
        {
            LLVMDumpModule(codegen->mod);
            const char* type_of_val_string   = LLVMPrintTypeToString(value_type);
            const char* type_to_match_string = LLVMPrintTypeToString(type_to_match);
            assert(false && "maybe_cast error");
        }
        return LLVMBuildBitCast(codegen->builder, value, type_to_match, "bit_cast");
    }
    return value;
}

///////////////////////////////////////////////////////
// Codegen
///////////////////////////////////////////////////////
// way to generate runtime functions / mappings from Machine?
void necro_codegen_declare_runtime_functions(NecroCodeGenLLVM* codegen, NecroMachineProgram* program)
{
    // NecroAlloc
    LLVMTypeRef  necro_alloc_args[4] = { LLVMInt64TypeInContext(codegen->context), LLVMInt16TypeInContext(codegen->context), LLVMInt16TypeInContext(codegen->context), LLVMInt8TypeInContext(codegen->context) };
    LLVMTypeRef  necro_alloc_type    = LLVMFunctionType(LLVMPointerType(LLVMInt64TypeInContext(codegen->context), 0), necro_alloc_args, 4, false);
    LLVMValueRef necro_alloc         = LLVMAddFunction(codegen->mod, "nalloc#", necro_alloc_type);
    LLVMSetLinkage(necro_alloc, LLVMExternalLinkage);
    LLVMSetFunctionCallConv(necro_alloc, LLVMCCallConv);
    NecroVar     nalloc_var          = necro_gen_var(program, NULL, "nalloc#", NECRO_NAME_UNIQUE);
    necro_codegen_symtable_get(codegen, nalloc_var)->type  = necro_alloc_type;
    necro_codegen_symtable_get(codegen, nalloc_var)->value = necro_alloc;
    codegen->nalloc_value            = necro_alloc;
    // codegen->runtime->functions.necro_alloc = necro_alloc;
}

// void necro_codegen_define_runtime_functions(NecroRuntime* runtime, LLVMExecutionEngineRef engine)
// {
//     necro_gc_init();
//     LLVMAddGlobalMapping(engine, runtime->functions.necro_alloc, _necro_alloc);
//     // LLVMAddGlobalMapping(engine, runtime->functions.necro_print, _necro_print);
//     // LLVMAddGlobalMapping(engine, runtime->functions.necro_sleep, _necro_sleep);
//     // LLVMAddGlobalMapping(engine, runtime->functions.necro_collect, _necro_collect);
//     // LLVMAddGlobalMapping(engine, runtime->functions.necro_initialize_root_set, _necro_initialize_root_set);
//     // LLVMAddGlobalMapping(engine, runtime->functions.necro_set_root, _necro_set_root);
//     // LLVMAddGlobalMapping(engine, runtime->functions.necro_error_exit, _necro_error_exit);
//     // LLVMAddGlobalMapping(engine, runtime->functions.necro_init_runtime, _necro_init_runtime);
//     // LLVMAddGlobalMapping(engine, runtime->functions.necro_update_runtime, _necro_update_runtime);
//     // LLVMAddGlobalMapping(engine, runtime->functions.necro_mouse_x, _necro_mouse_x);
//     // LLVMAddGlobalMapping(engine, runtime->functions.necro_mouse_y, _necro_mouse_y);
// }

void necro_codegen_function(NecroCodeGenLLVM* codegen, NecroMachineAST* ast)
{
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACHINE_FN_DEF);
    const char* fn_name = necro_intern_get_string(codegen->intern, ast->fn_def.name.symbol);
    assert(ast->fn_def.name.id.id != 0);

    // Fn begin
    LLVMTypeRef  fn_type  = necro_machine_type_to_llvm_type(codegen, ast->necro_machine_type);
    LLVMValueRef fn_value = LLVMAddFunction(codegen->mod, necro_intern_get_string(codegen->intern, ast->fn_def.name.symbol), fn_type);
    necro_codegen_symtable_get(codegen, ast->fn_def.name)->type  = fn_type;
    necro_codegen_symtable_get(codegen, ast->fn_def.name)->value = fn_value;
    necro_codegen_symtable_get(codegen, ast->fn_def.fn_value->value.reg_name)->type  = fn_type;
    necro_codegen_symtable_get(codegen, ast->fn_def.fn_value->value.reg_name)->value = fn_value;
    LLVMSetFunctionCallConv(fn_value, LLVMFastCallConv);
    LLVMBasicBlockRef entry = NULL;

    // Add all blocks
    NecroMachineAST* blocks = ast->fn_def.call_body;
    while (blocks != NULL)
    {
        LLVMBasicBlockRef block = LLVMAppendBasicBlock(fn_value, necro_intern_get_string(codegen->intern, blocks->block.name.symbol));
        necro_codegen_symtable_get(codegen, blocks->block.name)->block = block;
        if (entry == NULL)
            entry = block;
        blocks = blocks->block.next_block;
    }

    // codegen bodies
    blocks = ast->fn_def.call_body;
    while (blocks != NULL)
    {
        LLVMPositionBuilderAtEnd(codegen->builder, necro_codegen_symtable_get(codegen, blocks->block.name)->block);
        for (size_t i = 0; i < blocks->block.num_statements; ++i)
        {
            necro_codegen_block_statement(codegen, blocks->block.statements[i]);
        }
        necro_codegen_terminator(codegen, blocks->block.terminator);
        blocks = blocks->block.next_block;
    }
}

LLVMValueRef necro_codegen_terminator(NecroCodeGenLLVM* codegen, NecroTerminator* term)
{
    assert(codegen != NULL);
    assert(term != NULL);
    switch (term->type)
    {
    case NECRO_TERM_RETURN:
        return LLVMBuildRet(codegen->builder, necro_codegen_value(codegen, term->return_terminator.return_value));
    case NECRO_TERM_BREAK:
        return LLVMBuildBr(codegen->builder, necro_codegen_symtable_get(codegen, term->break_terminator.block_to_jump_to->block.name)->block);
    case NECRO_TERM_COND_BREAK:
        return LLVMBuildCondBr(codegen->builder, necro_codegen_value(codegen, term->cond_break_terminator.cond_value), necro_codegen_symtable_get(codegen, term->cond_break_terminator.true_block->block.name)->block, necro_codegen_symtable_get(codegen, term->cond_break_terminator.false_block->block.name)->block);
    case NECRO_TERM_SWITCH:
        return NULL;
    case NECRO_TERM_UNREACHABLE:
        return NULL;
    default:
        assert(false);
        return NULL;
    }
}

LLVMValueRef necro_codegen_nalloc(NecroCodeGenLLVM* codegen, NecroMachineAST* ast)
{
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACHINE_NALLOC);
    LLVMTypeRef  type          = necro_machine_type_to_llvm_type(codegen, ast->nalloc.type_to_alloc);
    uint64_t     data_size_n   = LLVMStoreSizeOfType(codegen->target, type);
    uint16_t     raw_slots     = ((((uint32_t)data_size_n) - sizeof(uint64_t)) / sizeof(int64_t*));
    uint16_t     slots_total_n = next_highest_pow_of_2(raw_slots) / 2;
    uint8_t      segment_n     = log2_32(slots_total_n);
    assert(ast->nalloc.slots_used <= slots_total_n);
    LLVMValueRef data_size     = LLVMConstInt(LLVMInt64TypeInContext(codegen->context), data_size_n, false);
    LLVMValueRef slots_used    = LLVMConstInt(LLVMInt16TypeInContext(codegen->context), ast->nalloc.slots_used, false);
    LLVMValueRef slots_total   = LLVMConstInt(LLVMInt16TypeInContext(codegen->context), slots_total_n, false);
    LLVMValueRef segment       = LLVMConstInt(LLVMInt8TypeInContext(codegen->context), segment_n, false);
    LLVMValueRef void_ptr      = LLVMBuildCall(codegen->builder, codegen->nalloc_value, (LLVMValueRef[]) { data_size, slots_used, slots_total, segment }, 4, "void_ptr");
    LLVMSetInstructionCallConv(void_ptr, LLVMCCallConv);
    LLVMValueRef data_ptr      = LLVMBuildBitCast(codegen->builder, void_ptr, LLVMPointerType(type, 0), "data_ptr");
    necro_codegen_symtable_get(codegen, ast->nalloc.result_reg->value.reg_name)->value = data_ptr;
    return data_ptr;
}

LLVMValueRef necro_codegen_value(NecroCodeGenLLVM* codegen, NecroMachineAST* ast)
{
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACHINE_VALUE);
    switch (ast->value.value_type)
    {
    case NECRO_MACHINE_VALUE_REG:              return necro_maybe_cast(codegen, necro_codegen_symtable_get(codegen, ast->value.reg_name)->value, necro_machine_type_to_llvm_type(codegen, ast->necro_machine_type));
    case NECRO_MACHINE_VALUE_PARAM:            return LLVMGetParam(necro_codegen_symtable_get(codegen, ast->value.param_reg.fn_name)->value, ast->value.param_reg.param_num);
    case NECRO_MACHINE_VALUE_UINT1_LITERAL:    return LLVMConstInt(LLVMInt1TypeInContext(codegen->context),  ast->value.uint1_literal,  false);
    case NECRO_MACHINE_VALUE_UINT8_LITERAL:    return LLVMConstInt(LLVMInt8TypeInContext(codegen->context),  ast->value.uint8_literal,  false);
    case NECRO_MACHINE_VALUE_UINT16_LITERAL:   return LLVMConstInt(LLVMInt16TypeInContext(codegen->context), ast->value.uint16_literal, false);
    case NECRO_MACHINE_VALUE_UINT32_LITERAL:   return LLVMConstInt(LLVMInt32TypeInContext(codegen->context), ast->value.uint32_literal, false);
    case NECRO_MACHINE_VALUE_UINT64_LITERAL:   return LLVMConstInt(LLVMInt64TypeInContext(codegen->context), ast->value.uint64_literal, false);
    case NECRO_MACHINE_VALUE_INT64_LITERAL:    return LLVMConstInt(LLVMInt64TypeInContext(codegen->context), ast->value.int64_literal,  true);
    case NECRO_MACHINE_VALUE_F64_LITERAL:      return LLVMConstReal(LLVMDoubleTypeInContext(codegen->context), ast->value.f64_literal);
    case NECRO_MACHINE_VALUE_NULL_PTR_LITERAL: return LLVMConstPointerNull(necro_machine_type_to_llvm_type(codegen, ast->necro_machine_type));
    case NECRO_MACHINE_VALUE_GLOBAL:
    {
        NecroMachineAST* global_ast = necro_symtable_get(codegen->symtable, ast->value.global_name.id)->necro_machine_ast;
        if (global_ast->type == NECRO_MACHINE_FN_DEF)
        {
            return necro_codegen_symtable_get(codegen, global_ast->fn_def.name)->value;
            // return necro_codegen_value(codegen, global_ast->fn_def.fn_value);
        }
        else if (global_ast->type == NECRO_MACHINE_DEF)
        {
            return necro_codegen_symtable_get(codegen, global_ast->machine_def.bind_name)->value;
        }
        else
        {
            // return necro_codegen_symtable_get(codegen, global_ast->machine_def.bind_name)->value;
            assert(false);
        }
        return NULL;
    }
    default: assert(false); return NULL;
    }
}

LLVMValueRef necro_codegen_store(NecroCodeGenLLVM* codegen, NecroMachineAST* ast)
{
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACHINE_STORE);
    LLVMValueRef source_value = necro_codegen_value(codegen, ast->store.source_value);
    switch (ast->store.store_type)
    {
    case NECRO_STORE_PTR:
    {
        LLVMValueRef dest_ptr = necro_codegen_value(codegen, ast->store.dest_ptr);
        return LLVMBuildStore(codegen->builder, necro_maybe_cast(codegen, source_value, LLVMGetElementType(LLVMTypeOf(dest_ptr))), dest_ptr);
    }
    case NECRO_STORE_SLOT:
    {
        LLVMValueRef dest_ptr_gep = LLVMBuildGEP(codegen->builder, necro_codegen_value(codegen, ast->store.dest_ptr), (LLVMValueRef[]) { LLVMConstInt(LLVMInt32TypeInContext(codegen->context), 0, false), LLVMConstInt(LLVMInt32TypeInContext(codegen->context), ast->store.store_slot.dest_slot, false) }, 2, "tmp");
        return LLVMBuildStore(codegen->builder, necro_maybe_cast(codegen, source_value, LLVMGetElementType(LLVMTypeOf(dest_ptr_gep))), dest_ptr_gep);
    }
    case NECRO_STORE_TAG:
    {
        LLVMValueRef dest_ptr_gep = LLVMBuildGEP(codegen->builder, necro_codegen_value(codegen, ast->store.dest_ptr), (LLVMValueRef[]) { LLVMConstInt(LLVMInt32TypeInContext(codegen->context), 0, false), LLVMConstInt(LLVMInt32TypeInContext(codegen->context), 0, false), LLVMConstInt(LLVMInt32TypeInContext(codegen->context), 1, false) }, 3, "tmp");
        return LLVMBuildStore(codegen->builder, source_value, dest_ptr_gep);
    }
    default: assert(false); return NULL;
    }
}

LLVMValueRef necro_codegen_load(NecroCodeGenLLVM* codegen, NecroMachineAST* ast)
{
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACHINE_LOAD);
    LLVMValueRef source_ptr = necro_codegen_value(codegen, ast->load.source_ptr);
    const char*  dest_name  = necro_intern_get_string(codegen->intern, ast->load.dest_value->value.reg_name.symbol);
    LLVMValueRef result     = NULL;
    switch (ast->load.load_type)
    {
    case NECRO_LOAD_PTR:   result = LLVMBuildLoad(codegen->builder, source_ptr, dest_name); break;
    case NECRO_LOAD_SLOT:
    {
        LLVMValueRef source_ptr_gep = LLVMBuildGEP(codegen->builder, source_ptr, (LLVMValueRef[]) { LLVMConstInt(LLVMInt32TypeInContext(codegen->context), 0, false), LLVMConstInt(LLVMInt32TypeInContext(codegen->context), ast->load.load_slot.source_slot, false) }, 2, dest_name);
        result                      = LLVMBuildLoad(codegen->builder, source_ptr_gep, dest_name);
        break;
    }
    case NECRO_LOAD_TAG:
    {
        LLVMValueRef source_ptr_gep = LLVMBuildGEP(codegen->builder, source_ptr, (LLVMValueRef[]) { LLVMConstInt(LLVMInt32TypeInContext(codegen->context), 0, false), LLVMConstInt(LLVMInt32TypeInContext(codegen->context), 0, false), LLVMConstInt(LLVMInt32TypeInContext(codegen->context), 1, false) }, 3, dest_name);
        result                      = LLVMBuildLoad(codegen->builder, source_ptr_gep, dest_name);
        break;
    }
    }
    necro_codegen_symtable_get(codegen, ast->load.dest_value->value.reg_name)->value = result;
    return result;
}

LLVMValueRef necro_codegen_bitcast(NecroCodeGenLLVM* codegen, NecroMachineAST* ast)
{
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACHINE_BIT_CAST);
    LLVMTypeRef  to_type  = necro_machine_type_to_llvm_type(codegen, ast->bit_cast.to_value->necro_machine_type);
    LLVMValueRef to_value = LLVMBuildBitCast(codegen->builder, necro_codegen_value(codegen, ast->bit_cast.from_value), to_type, "tmp");
    necro_codegen_symtable_get(codegen, ast->bit_cast.to_value->value.reg_name)->type  = to_type;
    necro_codegen_symtable_get(codegen, ast->bit_cast.to_value->value.reg_name)->value = to_value;
    return to_value;
}

LLVMValueRef necro_codegen_gep(NecroCodeGenLLVM* codegen, NecroMachineAST* ast)
{
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACHINE_GEP);
    const char*   name    = necro_intern_get_string(codegen->intern, ast->gep.dest_value->value.reg_name.symbol);
    LLVMValueRef  ptr     = necro_codegen_value(codegen, ast->gep.source_value);
    LLVMValueRef* indices = necro_paged_arena_alloc(&codegen->arena, ast->gep.num_indices * sizeof(LLVMValueRef));
    for (size_t i = 0; i < ast->gep.num_indices; ++i)
    {
        indices[i] = LLVMConstInt(LLVMInt32TypeInContext(codegen->context), ast->gep.indices[i], false);
    }
    LLVMValueRef value = LLVMBuildGEP(codegen->builder, ptr, indices, ast->gep.num_indices, name);
    necro_codegen_symtable_get(codegen, ast->gep.dest_value->value.reg_name)->type  = necro_machine_type_to_llvm_type(codegen, ast->gep.dest_value->necro_machine_type);
    necro_codegen_symtable_get(codegen, ast->gep.dest_value->value.reg_name)->value = value;
    return value;
}

LLVMValueRef necro_codegen_binop(NecroCodeGenLLVM* codegen, NecroMachineAST* ast)
{
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACHINE_BINOP);
    const char*  name  = necro_intern_get_string(codegen->intern, ast->binop.result->value.reg_name.symbol);
    LLVMValueRef value = NULL;
    LLVMValueRef left  = necro_codegen_value(codegen, ast->binop.left);
    LLVMValueRef right = necro_codegen_value(codegen, ast->binop.right);
    switch (ast->binop.binop_type)
    {
    case NECRO_MACHINE_BINOP_IADD: value = LLVMBuildAdd(codegen->builder, left, right, name); break;
    case NECRO_MACHINE_BINOP_ISUB: value = LLVMBuildSub(codegen->builder, left, right, name); break;
    case NECRO_MACHINE_BINOP_IMUL: value = LLVMBuildMul(codegen->builder, left, right, name); break;
    // case NECRO_MACHINE_BINOP_IDIV: value = LLVMBuildMul(codegen->builder, left, right, name); break;
    case NECRO_MACHINE_BINOP_FADD: value = LLVMBuildFAdd(codegen->builder, left, right, name); break;
    case NECRO_MACHINE_BINOP_FSUB: value = LLVMBuildFSub(codegen->builder, left, right, name); break;
    case NECRO_MACHINE_BINOP_FMUL: value = LLVMBuildFMul(codegen->builder, left, right, name); break;
    case NECRO_MACHINE_BINOP_FDIV: value = LLVMBuildFDiv(codegen->builder, left, right, name); break;
    default:                       assert(false); break;
    }
    necro_codegen_symtable_get(codegen, ast->binop.result->value.reg_name)->type  = necro_machine_type_to_llvm_type(codegen, ast->binop.result->necro_machine_type);
    necro_codegen_symtable_get(codegen, ast->binop.result->value.reg_name)->value = value;
    return value;
}

LLVMValueRef necro_codegen_cmp(NecroCodeGenLLVM* codegen, NecroMachineAST* ast)
{
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACHINE_CMP);
    const char*  name  = necro_intern_get_string(codegen->intern, ast->cmp.result->value.reg_name.symbol);
    LLVMValueRef left  = necro_codegen_value(codegen, ast->cmp.left);
    LLVMValueRef right = necro_codegen_value(codegen, ast->cmp.right);
    LLVMValueRef value = NULL;
    if (ast->cmp.left->type == NECRO_MACHINE_TYPE_UINT1  ||
        ast->cmp.left->type == NECRO_MACHINE_TYPE_UINT8  ||
        ast->cmp.left->type == NECRO_MACHINE_TYPE_UINT16 ||
        ast->cmp.left->type == NECRO_MACHINE_TYPE_UINT32 ||
        ast->cmp.left->type == NECRO_MACHINE_TYPE_UINT64 ||
        ast->cmp.left->type == NECRO_MACHINE_TYPE_PTR)
    {
        switch (ast->cmp.cmp_type)
        {
        case NECRO_MACHINE_CMP_EQ: value = LLVMBuildICmp(codegen->builder, LLVMIntEQ,  left, right, name); break;
        case NECRO_MACHINE_CMP_NE: value = LLVMBuildICmp(codegen->builder, LLVMIntNE,  left, right, name); break;
        case NECRO_MACHINE_CMP_GT: value = LLVMBuildICmp(codegen->builder, LLVMIntUGT, left, right, name); break;
        case NECRO_MACHINE_CMP_GE: value = LLVMBuildICmp(codegen->builder, LLVMIntUGE, left, right, name); break;
        case NECRO_MACHINE_CMP_LT: value = LLVMBuildICmp(codegen->builder, LLVMIntULT, left, right, name); break;
        case NECRO_MACHINE_CMP_LE: value = LLVMBuildICmp(codegen->builder, LLVMIntULE, left, right, name); break;
        default: assert(false); break;
        }
    }
    else if (ast->cmp.left->type == NECRO_MACHINE_TYPE_INT64)
    {
        switch (ast->cmp.cmp_type)
        {
        case NECRO_MACHINE_CMP_EQ: value = LLVMBuildICmp(codegen->builder, LLVMIntEQ,  left, right, name); break;
        case NECRO_MACHINE_CMP_NE: value = LLVMBuildICmp(codegen->builder, LLVMIntNE,  left, right, name); break;
        case NECRO_MACHINE_CMP_GT: value = LLVMBuildICmp(codegen->builder, LLVMIntSGT, left, right, name); break;
        case NECRO_MACHINE_CMP_GE: value = LLVMBuildICmp(codegen->builder, LLVMIntSGE, left, right, name); break;
        case NECRO_MACHINE_CMP_LT: value = LLVMBuildICmp(codegen->builder, LLVMIntSLT, left, right, name); break;
        case NECRO_MACHINE_CMP_LE: value = LLVMBuildICmp(codegen->builder, LLVMIntSLE, left, right, name); break;
        default: assert(false); break;
        }
    }
    else if (ast->cmp.left->type == NECRO_MACHINE_TYPE_F64)
    {
        switch (ast->cmp.cmp_type)
        {
        case NECRO_MACHINE_CMP_EQ: value = LLVMBuildICmp(codegen->builder, LLVMRealUEQ,  left, right, name); break;
        case NECRO_MACHINE_CMP_NE: value = LLVMBuildICmp(codegen->builder, LLVMRealUNE,  left, right, name); break;
        case NECRO_MACHINE_CMP_GT: value = LLVMBuildICmp(codegen->builder, LLVMRealUGT, left, right, name); break;
        case NECRO_MACHINE_CMP_GE: value = LLVMBuildICmp(codegen->builder, LLVMRealUGE, left, right, name); break;
        case NECRO_MACHINE_CMP_LT: value = LLVMBuildICmp(codegen->builder, LLVMRealULT, left, right, name); break;
        case NECRO_MACHINE_CMP_LE: value = LLVMBuildICmp(codegen->builder, LLVMRealULE, left, right, name); break;
        default: assert(false); break;
        }
    }
    else
    {
        assert(false);
    }
    necro_codegen_symtable_get(codegen, ast->cmp.result->value.reg_name)->type  = necro_machine_type_to_llvm_type(codegen, ast->cmp.result->necro_machine_type);
    necro_codegen_symtable_get(codegen, ast->cmp.result->value.reg_name)->value = value;
    return value;
}

LLVMValueRef necro_codegen_call(NecroCodeGenLLVM* codegen, NecroMachineAST* ast)
{
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACHINE_CALL);
    const char*   result_name = necro_intern_get_string(codegen->intern, ast->call.result_reg->value.reg_name.symbol);
    LLVMValueRef  fn_value    = necro_codegen_value(codegen, ast->call.fn_value);
    size_t        num_params  = ast->call.num_parameters;
    LLVMValueRef* params      = necro_paged_arena_alloc(&codegen->arena, num_params * sizeof(LLVMValueRef));
    for (size_t i = 0; i < num_params; ++i)
    {
        params[i] = necro_codegen_value(codegen, ast->call.parameters[i]);
    }
    LLVMValueRef result = LLVMBuildCall(codegen->builder, fn_value, params, num_params, result_name);
    LLVMSetInstructionCallConv(result, LLVMGetFunctionCallConv(fn_value));
    necro_codegen_symtable_get(codegen, ast->call.result_reg->value.reg_name)->type  = LLVMTypeOf(result);
    necro_codegen_symtable_get(codegen, ast->call.result_reg->value.reg_name)->value = result;
    return result;
}

LLVMValueRef necro_codegen_block_statement(NecroCodeGenLLVM* codegen, NecroMachineAST* ast)
{
    assert(codegen != NULL);
    assert(ast != NULL);
    switch (ast->type)
    {
    case NECRO_MACHINE_NALLOC:   return necro_codegen_nalloc(codegen, ast);
    case NECRO_MACHINE_VALUE:    return necro_codegen_value(codegen, ast);
    case NECRO_MACHINE_STORE:    return necro_codegen_store(codegen, ast);
    case NECRO_MACHINE_LOAD:     return necro_codegen_load(codegen, ast);
    case NECRO_MACHINE_BIT_CAST: return necro_codegen_bitcast(codegen, ast);
    case NECRO_MACHINE_GEP:      return necro_codegen_gep(codegen, ast);
    case NECRO_MACHINE_BINOP:    return necro_codegen_binop(codegen, ast);
    case NECRO_MACHINE_CALL:     return necro_codegen_call(codegen, ast);
    case NECRO_MACHINE_CMP:      return necro_codegen_cmp(codegen, ast);
    case NECRO_MACHINE_PHI:      return NULL;
    default:      assert(false); return NULL;
    }
}

void necro_codegen_global(NecroCodeGenLLVM* codegen, NecroMachineAST* ast)
{
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACHINE_VALUE);
    assert(ast->value.value_type == NECRO_MACHINE_VALUE_GLOBAL);
    LLVMTypeRef  global_type  = necro_machine_type_to_llvm_type(codegen, ast->necro_machine_type->ptr_type.element_type);
    const char*  global_name  = necro_intern_get_string(codegen->intern, ast->value.global_name.symbol);
    LLVMValueRef global_value = LLVMAddGlobal(codegen->mod, global_type, global_name);
    necro_codegen_symtable_get(codegen, ast->value.global_name)->type  = global_type;
    necro_codegen_symtable_get(codegen, ast->value.global_name)->value = global_value;
    LLVMSetLinkage(global_value, LLVMInternalLinkage);
    LLVMSetInitializer(global_value, necro_const_zero_machine_type(codegen, ast->necro_machine_type->ptr_type.element_type));
    LLVMSetGlobalConstant(global_value, false);
}

///////////////////////////////////////////////////////
// Necro Codegen Go
///////////////////////////////////////////////////////
NECRO_RETURN_CODE necro_codegen_llvm(NecroCodeGenLLVM* codegen, NecroMachineProgram* program)
{
    necro_codegen_declare_runtime_functions(codegen, program);
    codegen->poly_type = necro_machine_type_to_llvm_type(codegen, program->necro_poly_type);
    // Declare structs
    for (size_t i = 0; i < program->structs.length; ++i)
    {
        necro_machine_type_to_llvm_type(codegen, program->structs.data[i]->necro_machine_type);
    }
    // Declare machine structs
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        necro_machine_type_to_llvm_type(codegen, program->machine_defs.data[i]->necro_machine_type);
    }
    // codegen globals
    for (size_t i = 0; i < program->globals.length; ++i)
    {
        necro_codegen_global(codegen, program->globals.data[i]);
    }
    // codegen functions
    for (size_t i = 0; i < program->functions.length; ++i)
    {
        necro_codegen_function(codegen, program->functions.data[i]);
    }
    // codegen machine mk_fns
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        if (program->machine_defs.data[i]->machine_def.mk_fn != NULL)
            necro_codegen_function(codegen, program->machine_defs.data[i]->machine_def.mk_fn);
    }
    // codegen machine update_fns
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        necro_codegen_function(codegen, program->machine_defs.data[i]->machine_def.update_fn);
    }
    if (necro_verify_and_dump_codegen_llvm(codegen) == NECRO_ERROR)
        return NECRO_ERROR;
    return NECRO_SUCCESS;
}

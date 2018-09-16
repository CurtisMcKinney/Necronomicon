/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "codegen_llvm.h"
#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Transforms/IPO.h>
#include <llvm-c/Transforms/Scalar.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassManagerBuilder.h>
#include "symtable.h"
#include "runtime/runtime.h"

///////////////////////////////////////////////////////
// Create / Destroy
///////////////////////////////////////////////////////
NecroCodeGenLLVM necro_empty_codegen_llvm()
{
    return (NecroCodeGenLLVM)
    {
        .arena            = necro_paged_arena_empty(),
        .snapshot_arena   = necro_snapshot_arena_empty(),
        .intern           = NULL,
        .symtable         = NULL,
        .prim_types       = NULL,
        .codegen_symtable = necro_empty_necro_codegen_symbol_vector(),
        .runtime_mapping  = necro_empty_necro_runtime_mapping_vector(),
        .context          = NULL,
        .builder          = NULL,
        .mod              = NULL,
        .target           = NULL,
        .fn_pass_manager  = NULL,
        .mod_pass_manager = NULL,
        .should_optimize  = false,
        .memcpy_fn        = NULL,
        .memset_fn        = NULL,
    };
}

NecroCodeGenLLVM necro_create_codegen_llvm(NecroIntern* intern, NecroSymTable* symtable, NecroPrimTypes* prim_types, bool should_optimize)
{
    LLVMInitializeNativeTarget();

    LLVMContextRef       context          = LLVMContextCreate();
    LLVMModuleRef        mod              = LLVMModuleCreateWithNameInContext("necro", context);

    // LLVMTargetDataRef  target           = LLVMCreateTargetData(LLVMGetTarget(mod));
    // Target info
    // TODO: Dispose of what needs to be disposed!
    const char*          target_triple    = LLVMGetDefaultTargetTriple();
    const char*          target_cpu       = LLVMGetHostCPUName();
    const char*          target_features  = LLVMGetHostCPUFeatures();
    LLVMTargetRef        target           = NULL;
    char*                target_error;
    if (LLVMGetTargetFromTriple(target_triple, &target, &target_error))
    {
        fprintf(stderr, "necro error: %s\n", target_error);
        LLVMDisposeMessage(target_error);
        exit(1);
    }
    // LLVMCodeGenOptLevel  opt_level        = should_optimize ? LLVMCodeGenLevelAggressive : LLVMCodeGenLevelNone;
    LLVMCodeGenOptLevel  opt_level        = should_optimize ? LLVMCodeGenLevelDefault : LLVMCodeGenLevelNone;
    LLVMTargetMachineRef target_machine   = LLVMCreateTargetMachine(target, target_triple, target_cpu, target_features, opt_level, LLVMRelocDefault, LLVMCodeModelJITDefault);
    LLVMTargetDataRef    target_data      = LLVMCreateTargetDataLayout(target_machine);

    LLVMSetTarget(mod, target_triple);
    LLVMSetModuleDataLayout(mod, target_data);

    LLVMPassManagerRef fn_pass_manager  = LLVMCreateFunctionPassManagerForModule(mod);
    LLVMPassManagerRef mod_pass_manager = LLVMCreatePassManager();

    if (should_optimize)
    {
        LLVMInitializeFunctionPassManager(fn_pass_manager);
        LLVMAddDeadStoreEliminationPass(fn_pass_manager);
        LLVMAddAggressiveDCEPass(fn_pass_manager);
        LLVMAddInstructionCombiningPass(fn_pass_manager);
        LLVMAddCFGSimplificationPass(fn_pass_manager);
        LLVMAddTailCallEliminationPass(fn_pass_manager);
        LLVMAddCFGSimplificationPass(fn_pass_manager);
        LLVMAddDeadStoreEliminationPass(fn_pass_manager);
        LLVMAddAggressiveDCEPass(fn_pass_manager);
        LLVMAddInstructionCombiningPass(fn_pass_manager);
        LLVMFinalizeFunctionPassManager(fn_pass_manager);
        LLVMAddFunctionAttrsPass(mod_pass_manager);
        LLVMAddGlobalOptimizerPass(mod_pass_manager);
        LLVMPassManagerBuilderRef pass_manager_builder = LLVMPassManagerBuilderCreate();
        LLVMPassManagerBuilderSetOptLevel(pass_manager_builder, 0);
        // LLVMPassManagerBuilderUseInlinerWithThreshold(pass_manager_builder, 0);
        // LLVMPassManagerBuilderSetOptLevel(pass_manager_builder, opt_level);
        LLVMPassManagerBuilderUseInlinerWithThreshold(pass_manager_builder, 40);
        LLVMPassManagerBuilderPopulateFunctionPassManager(pass_manager_builder, fn_pass_manager);
        LLVMPassManagerBuilderPopulateModulePassManager(pass_manager_builder, mod_pass_manager);
    }
    else
    {
        LLVMInitializeFunctionPassManager(fn_pass_manager);
        LLVMAddCFGSimplificationPass(fn_pass_manager);
        LLVMAddTailCallEliminationPass(fn_pass_manager);
        LLVMFinalizeFunctionPassManager(fn_pass_manager);
        LLVMPassManagerBuilderRef pass_manager_builder = LLVMPassManagerBuilderCreate();
        LLVMPassManagerBuilderSetOptLevel(pass_manager_builder, 0);
        LLVMPassManagerBuilderPopulateFunctionPassManager(pass_manager_builder, fn_pass_manager);
        LLVMPassManagerBuilderPopulateModulePassManager(pass_manager_builder, mod_pass_manager);
    }
    return (NecroCodeGenLLVM)
    {
        .arena            = necro_paged_arena_create(),
        .snapshot_arena   = necro_snapshot_arena_create(),
        .intern           = intern,
        .symtable         = symtable,
        .prim_types       = prim_types,
        .codegen_symtable = necro_create_necro_codegen_symbol_vector(),
        .runtime_mapping  = necro_create_necro_runtime_mapping_vector(),
        .context          = context,
        .builder          = LLVMCreateBuilderInContext(context),
        .mod              = mod,
        .target           = target_data,
        .fn_pass_manager  = fn_pass_manager,
        .mod_pass_manager = mod_pass_manager,
        .should_optimize  = should_optimize,
        .memcpy_fn        = NULL,
        .memset_fn        = NULL,
    };
}

void necro_destroy_codegen_llvm(NecroCodeGenLLVM* codegen)
{
    assert(codegen != NULL);
    if (codegen->context != NULL)
        LLVMContextDispose(codegen->context);
    codegen->context = NULL;
    if (codegen->builder != NULL)
        LLVMDisposeBuilder(codegen->builder);
    codegen->builder = NULL;
    necro_paged_arena_destroy(&codegen->arena);
    necro_snapshot_arena_destroy(&codegen->snapshot_arena);
    necro_destroy_necro_codegen_symbol_vector(&codegen->codegen_symtable);
    necro_destroy_necro_runtime_mapping_vector(&codegen->runtime_mapping);
}

///////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////
LLVMTypeRef  necro_machine_type_to_llvm_type(NecroCodeGenLLVM* codegen, NecroMachineType* machine_type);
void         necro_codegen_function(NecroCodeGenLLVM* codegen, NecroMachineAST* ast);
LLVMValueRef necro_codegen_block_statement(NecroCodeGenLLVM* codegen, NecroMachineAST* ast);
LLVMValueRef necro_codegen_value(NecroCodeGenLLVM* codegen, NecroMachineAST* ast);
LLVMValueRef necro_codegen_terminator(NecroCodeGenLLVM* codegen, NecroTerminator* term, LLVMTypeRef return_type);

///////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////
void necro_print_codegen_llvm(NecroCodeGenLLVM* codegen)
{
    fflush(stdout);
    fflush(stderr);
    LLVMDumpModule(codegen->mod);
}

NECRO_RETURN_CODE necro_verify_and_dump_codegen_llvm(NecroCodeGenLLVM* codegen)
{
    char* error = NULL;
    // necro_print_codegen_llvm(codegen);
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
    case NECRO_MACHINE_TYPE_INT32:  return LLVMInt32TypeInContext(codegen->context);
    case NECRO_MACHINE_TYPE_INT64:  return LLVMInt64TypeInContext(codegen->context);
    case NECRO_MACHINE_TYPE_F32:    return LLVMFloatTypeInContext(codegen->context);
    case NECRO_MACHINE_TYPE_F64:    return LLVMDoubleTypeInContext(codegen->context);
    case NECRO_MACHINE_TYPE_CHAR:   assert(false); return NULL;
    case NECRO_MACHINE_TYPE_VOID:   return LLVMVoidTypeInContext(codegen->context);
    case NECRO_MACHINE_TYPE_PTR:    return LLVMPointerType(necro_machine_type_to_llvm_type(codegen, machine_type->ptr_type.element_type), 0);
    case NECRO_MACHINE_TYPE_STRUCT:
    {
        NecroCodeGenSymbolInfo* info = necro_codegen_symtable_get(codegen, machine_type->struct_type.name);
        if (info->type == NULL)
        {
            LLVMTypeRef struct_type = LLVMStructCreateNamed(codegen->context, machine_type->struct_type.name.symbol.str);
            info->type = struct_type;
            LLVMTypeRef* elements = necro_paged_arena_alloc(&codegen->arena, machine_type->struct_type.num_members * sizeof(LLVMTypeRef));
            for (size_t i = 0; i < machine_type->struct_type.num_members; ++i)
            {
                elements[i] = necro_machine_type_to_llvm_type(codegen, machine_type->struct_type.members[i]);
            }
            const size_t num_members = machine_type->struct_type.num_members;
            assert(num_members <= UINT32_MAX);
            LLVMStructSetBody(struct_type, elements, (uint32_t) num_members, false);
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

        const size_t num_parameters = machine_type->fn_type.num_parameters;
        assert(num_parameters <= UINT32_MAX);
        return LLVMFunctionType(return_type, parameters, (uint32_t) num_parameters, false);
    }
    default:
        assert(false);
        return NULL;
    }
}

bool necro_is_boxed_llvm_type(NecroCodeGenLLVM* codegen, LLVMTypeRef type)
{
    return type == codegen->word_int_type || type == codegen->word_uint_type || type == codegen->word_float_type;
}

LLVMValueRef necro_maybe_cast(NecroCodeGenLLVM* codegen, LLVMValueRef value, LLVMTypeRef type_to_match)
{
    LLVMTypeRef value_type = LLVMTypeOf(value);
    if (value_type != type_to_match)
    {
        if (value_type != codegen->poly_ptr_type && type_to_match != codegen->poly_ptr_type)
        {
            LLVMDumpModule(codegen->mod);
            const char* type_of_val_string   = LLVMPrintTypeToString(value_type);
            UNUSED(type_of_val_string);
            const char* type_to_match_string = LLVMPrintTypeToString(type_to_match);
            UNUSED(type_to_match_string);
            assert(false && "maybe_cast error");
        }
        // TODO: Test if it's a pointer or an unboxed type!!!
        // Float -> Poly
        if (value_type == codegen->word_float_type)
        {
            LLVMValueRef float_to_int = LLVMBuildBitCast(codegen->builder, value, codegen->word_uint_type, "float_to_int");
            return LLVMBuildIntToPtr(codegen->builder, float_to_int, type_to_match, "int_to_poly");
        }
        // Poly -> Float
        else if (type_to_match == codegen->word_float_type)
        {
            LLVMValueRef ptr_to_int = LLVMBuildPtrToInt(codegen->builder, value, codegen->word_uint_type, "poly_to_int");
            return LLVMBuildBitCast(codegen->builder, ptr_to_int, codegen->word_float_type, "int_to_float");
        }
        // Int -> Poly
        else if (necro_is_boxed_llvm_type(codegen, value_type))
        {
            return LLVMBuildIntToPtr(codegen->builder, value, type_to_match, "int_to_poly");
        }
        // Poly -> Int
        else if (necro_is_boxed_llvm_type(codegen, type_to_match))
        {
            return LLVMBuildPtrToInt(codegen->builder, value, type_to_match, "poly_to_int");
        }
        // Ptr -> Ptr
        else
        {
            return LLVMBuildBitCast(codegen->builder, value, type_to_match, "poly_cast");
        }
    }
    return value;
}

///////////////////////////////////////////////////////
// Codegen
///////////////////////////////////////////////////////
void necro_codegen_declare_function(NecroCodeGenLLVM* codegen, NecroMachineAST* ast)
{
    // Fn begin
    LLVMTypeRef  fn_type  = necro_machine_type_to_llvm_type(codegen, ast->necro_machine_type);
    LLVMValueRef fn_value = LLVMAddFunction(codegen->mod, ast->fn_def.name.symbol.str, fn_type);
    necro_codegen_symtable_get(codegen, ast->fn_def.name)->type  = fn_type;
    necro_codegen_symtable_get(codegen, ast->fn_def.name)->value = fn_value;
    necro_codegen_symtable_get(codegen, ast->fn_def.fn_value->value.reg_name)->type  = fn_type;
    necro_codegen_symtable_get(codegen, ast->fn_def.fn_value->value.reg_name)->value = fn_value;
}

void necro_codegen_function(NecroCodeGenLLVM* codegen, NecroMachineAST* ast)
{
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACHINE_FN_DEF);
    const char* fn_name = ast->fn_def.name.symbol.str;
    UNUSED(fn_name);
    assert(ast->fn_def.name.id.id != 0);

    // Fn begin
    LLVMTypeRef  fn_type  = necro_codegen_symtable_get(codegen, ast->fn_def.name)->type;
    LLVMValueRef fn_value = necro_codegen_symtable_get(codegen, ast->fn_def.name)->value;

    if (ast->fn_def.fn_type == NECRO_FN_RUNTIME)
    {
        // LLVMAVR
        LLVMAddAttributeAtIndex(fn_value, (LLVMAttributeIndex) LLVMAttributeFunctionIndex, LLVMCreateEnumAttribute(codegen->context, LLVMGetEnumAttributeKindForName("noinline", 8), 0));
        LLVMAddAttributeAtIndex(fn_value, (LLVMAttributeIndex) LLVMAttributeFunctionIndex, LLVMCreateEnumAttribute(codegen->context, LLVMGetEnumAttributeKindForName("optnone", 7), 0));
        // LLVMAddAttributeAtIndex(fn_value, LLVMAttributeFunctionIndex, LLVMCreateEnumAttribute(codegen->context, LLVMGetEnumAttributeKindForName("nodiscard", 9), 0));
        LLVMSetFunctionCallConv(fn_value, LLVMCCallConv);
        LLVMSetLinkage(fn_value, LLVMExternalLinkage);
        NecroRuntimeMapping mapping = (NecroRuntimeMapping) { .var = ast->fn_def.name,.value = fn_value,.addr = ast->fn_def.runtime_fn_addr };
        necro_push_necro_runtime_mapping_vector(&codegen->runtime_mapping, &mapping);
        return;
    }

    LLVMSetFunctionCallConv(fn_value, LLVMFastCallConv);
    LLVMBasicBlockRef entry = NULL;

    // Add all blocks
    NecroMachineAST* blocks = ast->fn_def.call_body;
    while (blocks != NULL)
    {
        LLVMBasicBlockRef block = LLVMAppendBasicBlock(fn_value, blocks->block.name.symbol.str);
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
        necro_codegen_terminator(codegen, blocks->block.terminator, LLVMGetReturnType(fn_type));
        blocks = blocks->block.next_block;
    }
    // if (codegen->should_optimize)
        LLVMRunFunctionPassManager(codegen->fn_pass_manager, fn_value);
}

LLVMValueRef necro_codegen_terminator(NecroCodeGenLLVM* codegen, NecroTerminator* term, LLVMTypeRef return_type)
{
    assert(codegen != NULL);
    assert(term != NULL);
    switch (term->type)
    {
    case NECRO_TERM_RETURN:
        return LLVMBuildRet(codegen->builder, necro_maybe_cast(codegen, necro_codegen_value(codegen, term->return_terminator.return_value), return_type));
    case NECRO_TERM_RETURN_VOID:
        return LLVMBuildRetVoid(codegen->builder);
    case NECRO_TERM_BREAK:
        return LLVMBuildBr(codegen->builder, necro_codegen_symtable_get(codegen, term->break_terminator.block_to_jump_to->block.name)->block);
    case NECRO_TERM_COND_BREAK:
        return LLVMBuildCondBr(codegen->builder, necro_codegen_value(codegen, term->cond_break_terminator.cond_value), necro_codegen_symtable_get(codegen, term->cond_break_terminator.true_block->block.name)->block, necro_codegen_symtable_get(codegen, term->cond_break_terminator.false_block->block.name)->block);
    case NECRO_TERM_UNREACHABLE:
        return LLVMBuildUnreachable(codegen->builder);
    case NECRO_TERM_SWITCH:
    {
        LLVMValueRef      cond_value   = necro_codegen_value(codegen, term->switch_terminator.choice_val);
        LLVMBasicBlockRef else_block   = necro_codegen_symtable_get(codegen, term->switch_terminator.else_block->block.name)->block;
        size_t            num_choices  = 0;
        NecroMachineSwitchList* choices = term->switch_terminator.values;
        while (choices != NULL)
        {
            num_choices++;
            choices = choices->next;
        }
        assert(num_choices <= UINT32_MAX);
        LLVMValueRef switch_value = LLVMBuildSwitch(codegen->builder, cond_value, else_block, (uint32_t) num_choices);
        choices = term->switch_terminator.values;
        while (choices != NULL)
        {
            LLVMValueRef      choice_val = LLVMConstInt(LLVMInt32TypeInContext(codegen->context), choices->data.value, false);
            LLVMBasicBlockRef block      = necro_codegen_symtable_get(codegen, choices->data.block->block.name)->block;
            LLVMAddCase(switch_value, choice_val, block);
            choices = choices->next;
        }
        return switch_value;
    }
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
    LLVMTypeRef  type      = necro_machine_type_to_llvm_type(codegen, ast->nalloc.type_to_alloc);
    // Add 1 word for header
    uint64_t     data_size = (ast->nalloc.slots_used + 1) * ((necro_word_size == NECRO_WORD_4_BYTES) ? 4 : 8);
    LLVMValueRef size_val  = (necro_word_size == NECRO_WORD_4_BYTES) ?
        LLVMConstInt(LLVMInt32TypeInContext(codegen->context), data_size, false) :
        LLVMConstInt(LLVMInt64TypeInContext(codegen->context), data_size, false);
    LLVMValueRef void_ptr  = // ast->nalloc.is_constant ?
        // LLVMBuildCall(codegen->builder, necro_codegen_symtable_get(codegen, codegen->necro_const_alloc_var)->value, (LLVMValueRef[]) { size_val }, 1, "void_ptr") :
        LLVMBuildCall(codegen->builder, necro_codegen_symtable_get(codegen, codegen->necro_from_alloc_var)->value, (LLVMValueRef[]) { size_val }, 1, "void_ptr");
    LLVMSetInstructionCallConv(void_ptr, LLVMCCallConv);
    LLVMValueRef data_ptr = LLVMBuildBitCast(codegen->builder, void_ptr, LLVMPointerType(type, 0), "data_ptr");
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
    case NECRO_MACHINE_VALUE_PARAM:
    {
        const size_t param_num = ast->value.param_reg.param_num;
        assert(param_num <= UINT32_MAX);
        return LLVMGetParam(necro_codegen_symtable_get(codegen, ast->value.param_reg.fn_name)->value, (uint32_t) param_num);
    }
    case NECRO_MACHINE_VALUE_UINT1_LITERAL:    return LLVMConstInt(LLVMInt1TypeInContext(codegen->context),  ast->value.uint1_literal,  false);
    case NECRO_MACHINE_VALUE_UINT8_LITERAL:    return LLVMConstInt(LLVMInt8TypeInContext(codegen->context),  ast->value.uint8_literal,  false);
    case NECRO_MACHINE_VALUE_UINT16_LITERAL:   return LLVMConstInt(LLVMInt16TypeInContext(codegen->context), ast->value.uint16_literal, false);
    case NECRO_MACHINE_VALUE_UINT32_LITERAL:   return LLVMConstInt(LLVMInt32TypeInContext(codegen->context), ast->value.uint32_literal, false);
    case NECRO_MACHINE_VALUE_UINT64_LITERAL:   return LLVMConstInt(LLVMInt64TypeInContext(codegen->context), ast->value.uint64_literal, false);
    case NECRO_MACHINE_VALUE_INT32_LITERAL:    return LLVMConstInt(LLVMInt32TypeInContext(codegen->context), ast->value.int32_literal,  true);
    case NECRO_MACHINE_VALUE_INT64_LITERAL:    return LLVMConstInt(LLVMInt64TypeInContext(codegen->context), ast->value.int64_literal,  true);
    case NECRO_MACHINE_VALUE_F32_LITERAL:      return LLVMConstReal(LLVMFloatTypeInContext(codegen->context), ast->value.f32_literal);
    case NECRO_MACHINE_VALUE_F64_LITERAL:      return LLVMConstReal(LLVMDoubleTypeInContext(codegen->context), ast->value.f64_literal);
    case NECRO_MACHINE_VALUE_NULL_PTR_LITERAL: return LLVMConstPointerNull(necro_machine_type_to_llvm_type(codegen, ast->necro_machine_type));
    case NECRO_MACHINE_VALUE_GLOBAL:           return necro_codegen_symtable_get(codegen, ast->value.global_name)->value;
    default:                    assert(false); return NULL;
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
    const char*  dest_name  = ast->load.dest_value->value.reg_name.symbol.str;
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

    LLVMValueRef value      = necro_codegen_value(codegen, ast->bit_cast.from_value);
    LLVMTypeRef  value_type = LLVMTypeOf(value);
    LLVMTypeRef  to_type    = necro_machine_type_to_llvm_type(codegen, ast->bit_cast.to_value->necro_machine_type);
    LLVMValueRef to_value   = NULL;

    const char* from_type_string = LLVMPrintTypeToString(value_type);
    UNUSED(from_type_string);
    const char* to_type_string   = LLVMPrintTypeToString(to_type);
    UNUSED(to_type_string);

    // TODO: Test if it's a pointer or an unboxed type!!!
    // Same Type
    if (value_type == to_type)
    {
        to_value = value;
    }
    // Float -> Ptr
    else if (value_type == codegen->word_float_type)
    {
        LLVMValueRef float_to_int = LLVMBuildBitCast(codegen->builder, value, codegen->word_uint_type, "float_to_int");
        to_value = LLVMBuildIntToPtr(codegen->builder, float_to_int, to_type, "int_to_ptr");
    }
    // Ptr -> Float
    else if (to_type == codegen->word_float_type)
    {
        LLVMValueRef ptr_to_int = LLVMBuildPtrToInt(codegen->builder, value, codegen->word_uint_type, "ptr_to_int");
        to_value = LLVMBuildBitCast(codegen->builder, ptr_to_int, codegen->word_float_type, "int_to_float");
    }
    // Int -> Ptr
    else if (necro_is_boxed_llvm_type(codegen, value_type))
    {
        to_value = LLVMBuildIntToPtr(codegen->builder, value, to_type, "int_to_ptr");
    }
    // Ptr -> Int
    else if (necro_is_boxed_llvm_type(codegen, to_type))
    {
        to_value = LLVMBuildPtrToInt(codegen->builder, value, to_type, "ptr_to_int");
    }
    // Everything else
    else
    {
        to_value = LLVMBuildBitCast(codegen->builder, value, to_type, "bit_cast");
    }

    necro_codegen_symtable_get(codegen, ast->bit_cast.to_value->value.reg_name)->type  = to_type;
    necro_codegen_symtable_get(codegen, ast->bit_cast.to_value->value.reg_name)->value = to_value;

    return to_value;
}

LLVMValueRef necro_codegen_zext(NecroCodeGenLLVM* codegen, NecroMachineAST* ast)
{
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACHINE_ZEXT);
    LLVMValueRef value = LLVMBuildZExt(codegen->builder, necro_codegen_value(codegen, ast->zext.from_value), necro_machine_type_to_llvm_type(codegen, ast->zext.to_value->necro_machine_type), "zxt");
    necro_codegen_symtable_get(codegen, ast->zext.to_value->value.reg_name)->type  = LLVMTypeOf(value);
    necro_codegen_symtable_get(codegen, ast->zext.to_value->value.reg_name)->value = value;
    return value;
}

LLVMValueRef necro_codegen_gep(NecroCodeGenLLVM* codegen, NecroMachineAST* ast)
{
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACHINE_GEP);
    const char*   name    = ast->gep.dest_value->value.reg_name.symbol.str;
    LLVMValueRef  ptr     = necro_codegen_value(codegen, ast->gep.source_value);
    LLVMValueRef* indices = necro_paged_arena_alloc(&codegen->arena, ast->gep.num_indices * sizeof(LLVMValueRef));
    for (size_t i = 0; i < ast->gep.num_indices; ++i)
    {
        // indices[i] = LLVMConstInt(LLVMInt32TypeInContext(codegen->context), ast->gep.indices[i], false);
        indices[i] = necro_codegen_value(codegen, ast->gep.indices[i]);
    }
    const size_t num_indices = ast->gep.num_indices;
    assert(num_indices <= UINT32_MAX);
    LLVMValueRef value = LLVMBuildGEP(codegen->builder, ptr, indices, (uint32_t) num_indices, name);
    necro_codegen_symtable_get(codegen, ast->gep.dest_value->value.reg_name)->type  = necro_machine_type_to_llvm_type(codegen, ast->gep.dest_value->necro_machine_type);
    necro_codegen_symtable_get(codegen, ast->gep.dest_value->value.reg_name)->value = value;
    return value;
}

LLVMValueRef necro_codegen_binop(NecroCodeGenLLVM* codegen, NecroMachineAST* ast)
{
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACHINE_BINOP);
    const char*  name  = ast->binop.result->value.reg_name.symbol.str;
    LLVMValueRef value = NULL;
    LLVMValueRef left  = necro_codegen_value(codegen, ast->binop.left);
    LLVMValueRef right = necro_codegen_value(codegen, ast->binop.right);
    switch (ast->binop.binop_type)
    {
    case NECRO_MACHINE_BINOP_IADD: value = LLVMBuildAdd(codegen->builder, left, right, name);  break;
    case NECRO_MACHINE_BINOP_ISUB: value = LLVMBuildSub(codegen->builder, left, right, name);  break;
    case NECRO_MACHINE_BINOP_IMUL: value = LLVMBuildMul(codegen->builder, left, right, name);  break;
    // case NECRO_MACHINE_BINOP_IDIV: value = LLVMBuildMul(codegen->builder, left, right, name); break;
    case NECRO_MACHINE_BINOP_OR:   value = LLVMBuildOr(codegen->builder, left, right, name);   break;
    case NECRO_MACHINE_BINOP_AND:  value = LLVMBuildAnd(codegen->builder, left, right, name);  break;
    case NECRO_MACHINE_BINOP_SHL:  value = LLVMBuildShl(codegen->builder, left, right, name);  break;
    case NECRO_MACHINE_BINOP_SHR:  value = LLVMBuildLShr(codegen->builder, left, right, name); break;
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
    const char*  name  = ast->cmp.result->value.reg_name.symbol.str;
    LLVMValueRef left  = necro_codegen_value(codegen, ast->cmp.left);
    LLVMValueRef right = necro_codegen_value(codegen, ast->cmp.right);
    LLVMValueRef value = NULL;
    if (ast->cmp.left->necro_machine_type->type == NECRO_MACHINE_TYPE_UINT1  ||
        ast->cmp.left->necro_machine_type->type == NECRO_MACHINE_TYPE_UINT8  ||
        ast->cmp.left->necro_machine_type->type == NECRO_MACHINE_TYPE_UINT16 ||
        ast->cmp.left->necro_machine_type->type == NECRO_MACHINE_TYPE_UINT32 ||
        ast->cmp.left->necro_machine_type->type == NECRO_MACHINE_TYPE_UINT64 ||
        ast->cmp.left->necro_machine_type->type == NECRO_MACHINE_TYPE_PTR)
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
    else if (ast->cmp.left->necro_machine_type->type == NECRO_MACHINE_TYPE_INT32 ||
             ast->cmp.left->necro_machine_type->type == NECRO_MACHINE_TYPE_INT64)
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
    else if (ast->cmp.left->necro_machine_type->type == NECRO_MACHINE_TYPE_F32 ||
             ast->cmp.left->necro_machine_type->type == NECRO_MACHINE_TYPE_F64)
    {
        switch (ast->cmp.cmp_type)
        {
        case NECRO_MACHINE_CMP_EQ: value = LLVMBuildFCmp(codegen->builder, LLVMRealUEQ,  left, right, name); break;
        case NECRO_MACHINE_CMP_NE: value = LLVMBuildFCmp(codegen->builder, LLVMRealUNE,  left, right, name); break;
        case NECRO_MACHINE_CMP_GT: value = LLVMBuildFCmp(codegen->builder, LLVMRealUGT, left, right, name); break;
        case NECRO_MACHINE_CMP_GE: value = LLVMBuildFCmp(codegen->builder, LLVMRealUGE, left, right, name); break;
        case NECRO_MACHINE_CMP_LT: value = LLVMBuildFCmp(codegen->builder, LLVMRealULT, left, right, name); break;
        case NECRO_MACHINE_CMP_LE: value = LLVMBuildFCmp(codegen->builder, LLVMRealULE, left, right, name); break;
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
    NecroArenaSnapshot snapshot    = necro_snapshot_arena_get(&codegen->snapshot_arena);
    bool               is_void     = ast->call.result_reg->value.value_type == NECRO_MACHINE_VALUE_VOID;
    const char*        result_name = NULL;
    if (!is_void)
        result_name = ast->call.result_reg->value.reg_name.symbol.str;
    else
        result_name = "";
    LLVMValueRef  fn_value    = necro_codegen_value(codegen, ast->call.fn_value);
    size_t        num_params  = ast->call.num_parameters;
    LLVMValueRef* params      = necro_paged_arena_alloc(&codegen->arena, num_params * sizeof(LLVMValueRef));
    LLVMTypeRef*  param_types = necro_paged_arena_alloc(&codegen->arena, num_params * sizeof(LLVMTypeRef));
    LLVMTypeRef   fn_value_ty = LLVMGetElementType(LLVMTypeOf(fn_value));
    // const char*   fn_ty_str   = LLVMPrintTypeToString(fn_value_ty);
    LLVMGetParamTypes(fn_value_ty, param_types);
    for (size_t i = 0; i < num_params; ++i)
    {
        params[i] = necro_maybe_cast(codegen, necro_codegen_value(codegen, ast->call.parameters[i]), param_types[i]);
    }
    assert(num_params <= UINT32_MAX);
    LLVMValueRef result = LLVMBuildCall(codegen->builder, fn_value, params, (uint32_t) num_params, result_name);
    if (ast->call.call_type == NECRO_C_CALL)
        LLVMSetInstructionCallConv(result, LLVMCCallConv);
    else
        LLVMSetInstructionCallConv(result, LLVMFastCallConv);
    if (!is_void)
    {
        necro_codegen_symtable_get(codegen, ast->call.result_reg->value.reg_name)->type  = LLVMTypeOf(result);
        necro_codegen_symtable_get(codegen, ast->call.result_reg->value.reg_name)->value = result;
    }
    necro_snapshot_arena_rewind(&codegen->snapshot_arena, snapshot);
    return result;
}

LLVMValueRef necro_codegen_phi(NecroCodeGenLLVM* codegen, NecroMachineAST* ast)
{
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACHINE_PHI);
    LLVMTypeRef          phi_type  = necro_machine_type_to_llvm_type(codegen, ast->phi.result->necro_machine_type);
    LLVMValueRef         phi_value = LLVMBuildPhi(codegen->builder, phi_type, ast->phi.result->value.reg_name.symbol.str);
    NecroMachinePhiList* values    = ast->phi.values;
    while (values != NULL)
    {
        LLVMValueRef      leaf_value = necro_codegen_value(codegen, values->data.value);
        LLVMBasicBlockRef block      = necro_codegen_symtable_get(codegen, values->data.block->block.name)->block;
        char* phi_type_string = LLVMPrintTypeToString(phi_type);
        UNUSED(phi_type_string);
        char* val_type_string = LLVMPrintTypeToString(LLVMTypeOf(leaf_value));
        UNUSED(val_type_string);
        LLVMAddIncoming(phi_value, &leaf_value, &block, 1);
        values = values->next;
    }
    necro_codegen_symtable_get(codegen, ast->phi.result->value.reg_name)->type  = phi_type;
    necro_codegen_symtable_get(codegen, ast->phi.result->value.reg_name)->value = phi_value;
    return phi_value;
}

LLVMValueRef necro_codegen_memcpy(NecroCodeGenLLVM* codegen, NecroMachineAST* ast)
{
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACHINE_MEMCPY);
    LLVMTypeRef word_int = (necro_word_size == NECRO_WORD_4_BYTES) ? LLVMInt32TypeInContext(codegen->context) : LLVMInt64TypeInContext(codegen->context);
    if (codegen->memcpy_fn == NULL)
    {
        if (necro_word_size == NECRO_WORD_4_BYTES)
        {
            LLVMTypeRef memcpy_type = LLVMFunctionType(LLVMVoidTypeInContext(codegen->context), (LLVMTypeRef[]) { LLVMPointerType(word_int, 0), LLVMPointerType(word_int, 0),  word_int, LLVMInt1TypeInContext(codegen->context) }, 4, false);
            codegen->memcpy_fn      = LLVMAddFunction(codegen->mod, "llvm.memcpy.p0i32.p0i32.i32", memcpy_type);
        }
        else
        {
            LLVMTypeRef memcpy_type = LLVMFunctionType(LLVMVoidTypeInContext(codegen->context), (LLVMTypeRef[]) { LLVMPointerType(word_int, 0), LLVMPointerType(word_int, 0),  word_int, LLVMInt1TypeInContext(codegen->context) }, 4, false);
            codegen->memcpy_fn      = LLVMAddFunction(codegen->mod, "llvm.memcpy.p0i64.p0i64.i64", memcpy_type);
        }
        LLVMSetFunctionCallConv(codegen->memcpy_fn, LLVMFastCallConv);
    }
    LLVMValueRef dst         = necro_codegen_value(codegen, ast->memcpy.dest);
    dst                      = LLVMBuildBitCast(codegen->builder, dst, LLVMPointerType(word_int, 0), "dst");
    LLVMValueRef src         = necro_codegen_value(codegen, ast->memcpy.source);
    src                      = LLVMBuildBitCast(codegen->builder, src, LLVMPointerType(word_int, 0), "src");
    size_t       data_size   = (size_t) LLVMStoreSizeOfType(codegen->target, necro_machine_type_to_llvm_type(codegen, ast->memcpy.dest->necro_machine_type->ptr_type.element_type));
    LLVMValueRef size_val    = LLVMConstInt(word_int, data_size, false);
    LLVMValueRef is_volatile = LLVMConstInt(LLVMInt1TypeInContext(codegen->context), 0, false);
    LLVMValueRef memcpy_val  =  LLVMBuildCall(codegen->builder, codegen->memcpy_fn, (LLVMValueRef[]) { dst, src, size_val, is_volatile }, 4, "");
    LLVMSetInstructionCallConv(memcpy_val, LLVMGetFunctionCallConv(codegen->memcpy_fn));
    return memcpy_val;
}

LLVMValueRef necro_codegen_memset(NecroCodeGenLLVM* codegen, NecroMachineAST* ast)
{
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACHINE_MEMSET);
    LLVMTypeRef word_int = (necro_word_size == NECRO_WORD_4_BYTES) ? LLVMInt32TypeInContext(codegen->context) : LLVMInt64TypeInContext(codegen->context);
    if (codegen->memset_fn == NULL)
    {
        if (necro_word_size == NECRO_WORD_4_BYTES)
        {
            LLVMTypeRef memset_type = LLVMFunctionType(LLVMVoidTypeInContext(codegen->context), (LLVMTypeRef[]) { LLVMPointerType(word_int, 0), LLVMInt8TypeInContext(codegen->context), word_int, LLVMInt1TypeInContext(codegen->context) }, 4, false);
            codegen->memset_fn      = LLVMAddFunction(codegen->mod, "llvm.memset.p0i32.i32", memset_type);
        }
        else
        {
            LLVMTypeRef memset_type = LLVMFunctionType(LLVMVoidTypeInContext(codegen->context), (LLVMTypeRef[]) { LLVMPointerType(word_int, 0), LLVMInt8TypeInContext(codegen->context), word_int, LLVMInt1TypeInContext(codegen->context) }, 4, false);
            codegen->memset_fn      = LLVMAddFunction(codegen->mod, "llvm.memset.p0i64.i64", memset_type);
        }

        LLVMSetFunctionCallConv(codegen->memset_fn, LLVMFastCallConv);
        // LLVMSetInstructionCallConv(memset_val, LLVMGetFunctionCallConv(codegen->memset_fn));
    }
    LLVMValueRef ptr         = necro_codegen_value(codegen, ast->memset.ptr);
    ptr                      = LLVMBuildBitCast(codegen->builder, ptr, LLVMPointerType(word_int, 0), "ptr");
    LLVMValueRef value       = necro_codegen_value(codegen, ast->memset.value);
    LLVMValueRef num_bytes   = necro_codegen_value(codegen, ast->memset.num_bytes);
    LLVMValueRef is_volatile = LLVMConstInt(LLVMInt1TypeInContext(codegen->context), 1, false);
    // src                      = LLVMBuildBitCast(codegen->builder, src, LLVMPointerType(word_int, 0), "src");
    // size_t       data_size   = (size_t) LLVMStoreSizeOfType(codegen->target, necro_machine_type_to_llvm_type(codegen, ast->memcpy.dest->necro_machine_type->ptr_type.element_type));
    // LLVMValueRef size_val    = LLVMConstInt(word_int, data_size, false);
    // LLVMValueRef is_volatile = LLVMConstInt(LLVMInt1TypeInContext(codegen->context), 0, false);
    const char* type_of_val_string   = LLVMPrintTypeToString(LLVMTypeOf(codegen->memset_fn));
    UNUSED(type_of_val_string);
    LLVMValueRef memset_val  =  LLVMBuildCall(codegen->builder, codegen->memset_fn, (LLVMValueRef[]) { ptr, value, num_bytes, is_volatile }, 4, "");
    LLVMSetInstructionCallConv(memset_val, LLVMGetFunctionCallConv(codegen->memset_fn));
    return memset_val;
}

LLVMValueRef necro_codegen_alloca(NecroCodeGenLLVM* codegen, NecroMachineAST* ast)
{
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACHINE_ALLOCA);
    LLVMValueRef result = LLVMBuildArrayAlloca(codegen->builder, codegen->poly_ptr_type, LLVMConstInt(LLVMInt32TypeInContext(codegen->context), ast->alloca.num_slots, false), "alloca");
    necro_codegen_symtable_get(codegen, ast->alloca.result->value.reg_name)->type  = LLVMTypeOf(result);
    necro_codegen_symtable_get(codegen, ast->alloca.result->value.reg_name)->value = result;
    return result;
}

LLVMValueRef necro_codegen_select(NecroCodeGenLLVM* codegen, NecroMachineAST* ast)
{
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACHINE_SELECT);
    LLVMValueRef result = LLVMBuildSelect(codegen->builder, necro_codegen_value(codegen, ast->select.cmp_value), necro_codegen_value(codegen, ast->select.left), necro_codegen_value(codegen, ast->select.right), "sel_result");
    necro_codegen_symtable_get(codegen, ast->select.result->value.reg_name)->type  = LLVMTypeOf(result);
    necro_codegen_symtable_get(codegen, ast->select.result->value.reg_name)->value = result;
    return result;
}

LLVMValueRef necro_codegen_block_statement(NecroCodeGenLLVM* codegen, NecroMachineAST* ast)
{
    assert(codegen != NULL);
    assert(ast != NULL);
    switch (ast->type)
    {
    case NECRO_MACHINE_NALLOC:   return necro_codegen_nalloc(codegen, ast);
    case NECRO_MACHINE_ALLOCA:   return necro_codegen_alloca(codegen, ast);
    case NECRO_MACHINE_VALUE:    return necro_codegen_value(codegen, ast);
    case NECRO_MACHINE_STORE:    return necro_codegen_store(codegen, ast);
    case NECRO_MACHINE_LOAD:     return necro_codegen_load(codegen, ast);
    case NECRO_MACHINE_BIT_CAST: return necro_codegen_bitcast(codegen, ast);
    case NECRO_MACHINE_ZEXT:     return necro_codegen_zext(codegen, ast);
    case NECRO_MACHINE_GEP:      return necro_codegen_gep(codegen, ast);
    case NECRO_MACHINE_BINOP:    return necro_codegen_binop(codegen, ast);
    case NECRO_MACHINE_CALL:     return necro_codegen_call(codegen, ast);
    case NECRO_MACHINE_CMP:      return necro_codegen_cmp(codegen, ast);
    case NECRO_MACHINE_PHI:      return necro_codegen_phi(codegen, ast);
    case NECRO_MACHINE_MEMCPY:   return necro_codegen_memcpy(codegen, ast);
    case NECRO_MACHINE_MEMSET:   return necro_codegen_memset(codegen, ast);
    case NECRO_MACHINE_SELECT:   return necro_codegen_select(codegen, ast);
    default:                     assert(false); return NULL;
    }
}

void necro_codegen_global(NecroCodeGenLLVM* codegen, NecroMachineAST* ast)
{
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACHINE_VALUE);
    assert(ast->value.value_type == NECRO_MACHINE_VALUE_GLOBAL);
    LLVMTypeRef  global_type  = necro_machine_type_to_llvm_type(codegen, ast->necro_machine_type->ptr_type.element_type);
    LLVMValueRef zero_value   = LLVMConstNull(global_type);
    const char*  global_name  = ast->value.global_name.symbol.str;
    LLVMValueRef global_value = LLVMAddGlobal(codegen->mod, global_type, global_name);
    necro_codegen_symtable_get(codegen, ast->value.global_name)->type  = global_type;
    necro_codegen_symtable_get(codegen, ast->value.global_name)->value = global_value;
    LLVMSetLinkage(global_value, LLVMInternalLinkage);
    LLVMSetInitializer(global_value, zero_value);
    LLVMSetGlobalConstant(global_value, false);
}

///////////////////////////////////////////////////////
// Necro Codegen Go
///////////////////////////////////////////////////////
NECRO_RETURN_CODE necro_codegen_llvm(NecroCodeGenLLVM* codegen, NecroMachineProgram* program)
{
    // cache useful things
    codegen->member_map            = &program->copy_table.member_map;
    codegen->data_map              = &program->copy_table.data_map;
    codegen->poly_type             = necro_machine_type_to_llvm_type(codegen, program->necro_poly_type);
    codegen->poly_ptr_type         = necro_machine_type_to_llvm_type(codegen, program->necro_poly_ptr_type);
    codegen->word_int_type         = necro_machine_type_to_llvm_type(codegen, program->necro_int_type);
    codegen->word_uint_type        = necro_machine_type_to_llvm_type(codegen, program->necro_uint_type);
    codegen->word_float_type       = necro_machine_type_to_llvm_type(codegen, program->necro_float_type);
    codegen->necro_from_alloc_var  = program->runtime._necro_from_alloc;
    // codegen->necro_const_alloc_var = program->runtime._necro_const_alloc;
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
    // Declare functions
    for (size_t i = 0; i < program->functions.length; ++i)
    {
        necro_codegen_declare_function(codegen, program->functions.data[i]);
    }
    // Declare machine init_fns
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        if (program->machine_defs.data[i]->machine_def.init_fn != NULL)
            necro_codegen_declare_function(codegen, program->machine_defs.data[i]->machine_def.init_fn);
    }
    // Declare machine mk_fns
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        if (program->machine_defs.data[i]->machine_def.mk_fn != NULL)
            necro_codegen_declare_function(codegen, program->machine_defs.data[i]->machine_def.mk_fn);
    }
    // Declare machine update_fns
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        necro_codegen_declare_function(codegen, program->machine_defs.data[i]->machine_def.update_fn);
    }
    // Declare main_fn
    necro_codegen_declare_function(codegen, program->necro_main);
    // codegen functions
    for (size_t i = 0; i < program->functions.length; ++i)
    {
        necro_codegen_function(codegen, program->functions.data[i]);
    }
    // codegen machine init_fns
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        if (program->machine_defs.data[i]->machine_def.init_fn != NULL)
            necro_codegen_function(codegen, program->machine_defs.data[i]->machine_def.init_fn);
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
    // codegen main
    necro_codegen_function(codegen, program->necro_main);
    if (codegen->should_optimize)
        LLVMRunPassManager(codegen->mod_pass_manager, codegen->mod);
    // verify
    if (necro_verify_and_dump_codegen_llvm(codegen) == NECRO_ERROR)
        return NECRO_ERROR;
    return NECRO_SUCCESS;
}

///////////////////////////////////////////////////////
// Necro JIT
///////////////////////////////////////////////////////
NECRO_RETURN_CODE necro_jit_llvm(NecroCodeGenLLVM* codegen)
{
    LLVMLinkInMCJIT();
    LLVMInitializeNativeAsmPrinter();
    // LLVMInitializeNativeAsmParser();
    LLVMExecutionEngineRef engine;
    char* error = NULL;
    if (LLVMCreateJITCompilerForModule(&engine, codegen->mod, 0, &error) != 0)
    {
        fprintf(stderr, "necro error: %s\n", error);
        LLVMDisposeMessage(error);
        return NECRO_ERROR;
    }

    // Map runtime functions
    for (size_t i = 0; i < codegen->runtime_mapping.length; ++i)
    {
        if (codegen->runtime_mapping.data[i].value != NULL &&
            !LLVMIsNull(codegen->runtime_mapping.data[i].value) &&
            LLVMIsAFunction(codegen->runtime_mapping.data[i].value))
            LLVMAddGlobalMapping(engine, codegen->runtime_mapping.data[i].value, codegen->runtime_mapping.data[i].addr);
    }

    // TODO / HACK: Manual data info set up.
    // Ideally this should be done via the actual application logic...
    _necro_set_data_map(codegen->data_map->data);
    _necro_set_member_map(codegen->member_map->data);
    necro_copy_gc_init();
    void(*fun)() = (void(*)())((size_t)LLVMGetFunctionAddress(engine, "_necro_main"));
    assert(fun != NULL);

#ifdef _WIN32
    system("cls");
#endif

    puts("\n\n");
    puts("__/\\\\/\\\\\\\\\\\\_______/\\\\\\\\\\\\\\\\______/\\\\\\\\\\\\\\\\__/\\\\/\\\\\\\\\\\\\\______/\\\\\\\\\\____ ");
    puts(" _\\/\\\\\\////\\\\\\____/\\\\\\/////\\\\\\___/\\\\\\//////__\\/\\\\\\/////\\\\\\___/\\\\\\///\\\\\\__");
    puts("  _\\/\\\\\\__\\//\\\\\\__/\\\\\\\\\\\\\\\\\\\\\\___/\\\\\\_________\\/\\\\\\___\\///___/\\\\\\__\\//\\\\\\");
    puts("   _\\/\\\\\\___\\/\\\\\\_\\//\\\\///////___\\//\\\\\\________\\/\\\\\\_________\\//\\\\\\__/\\\\\\");
    puts("    _\\/\\\\\\___\\/\\\\\\__\\//\\\\\\\\\\\\\\\\\\\\__\\///\\\\\\\\\\\\\\\\_\\/\\\\\\__________\\///\\\\\\\\\\/");
    puts("     _\\///____\\///____\\//////////_____\\////////__\\///_____________\\/////_");
    puts("\n");
    puts("    by Curtis McKinney (Somniloquist)");
    puts("    and Chad McKinney (SeppukuZombie)");
    puts("\n");
    (*fun)();
    LLVMDisposeExecutionEngine(engine);
    necro_copy_gc_cleanup();
    return NECRO_SUCCESS;
}

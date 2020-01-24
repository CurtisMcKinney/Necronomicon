/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "codegen_llvm.h"
#include <math.h>
#include <ctype.h>
#include <stdio.h>

#include <llvm-c/Analysis.h>
#include <llvm-c/Transforms/IPO.h>
#include <llvm-c/Transforms/Scalar.h>
#include <llvm-c/Transforms/Vectorize.h>
#include <llvm-c/Transforms/Utils.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassManagerBuilder.h>
#include <llvm-c/Support.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/DebugInfo.h>

#include "alias_analysis.h"
#include "core_ast.h"
#include "core_infer.h"
#include "monomorphize.h"
#include "core_simplify.h"
#include "lambda_lift.h"
#include "defunctionalization.h"
#include "state_analysis.h"
#include "mach_transform.h"
#include "mach_print.h"
#include "runtime.h"

/*

    Performance / Optimizations Reference:
        * https://www.agner.org/optimize/

*/

///////////////////////////////////////////////////////
// NecroLLVM
///////////////////////////////////////////////////////
typedef struct NecroLLVMSymbol
{
    NecroMachAstSymbol* mach_symbol;
    LLVMTypeRef         type;
    LLVMValueRef        value;
    LLVMBasicBlockRef   block;
} NecroLLVMSymbol;

NecroLLVMSymbol* necro_llvm_symbol_create(NecroPagedArena* arena, LLVMTypeRef type, LLVMValueRef value, LLVMBasicBlockRef block, NecroMachAstSymbol* mach_symbol)
{
    NecroLLVMSymbol* llvm_symbol = necro_paged_arena_alloc(arena, sizeof(NecroLLVMSymbol));
    llvm_symbol->mach_symbol     = mach_symbol;
    llvm_symbol->type            = type;
    llvm_symbol->value           = value;
    llvm_symbol->block           = block;
    return llvm_symbol;
}

NecroLLVMSymbol* necro_llvm_symbol_get(NecroPagedArena* arena, NecroMachAstSymbol* mach_symbol)
{
    assert(mach_symbol != NULL);
    if (mach_symbol->codegen_symbol == NULL)
    {
        NecroLLVMSymbol* llvm_symbol = necro_llvm_symbol_create(arena, NULL, NULL, NULL, mach_symbol);
        mach_symbol->codegen_symbol  = (void*) llvm_symbol;
    }
    return (NecroLLVMSymbol*) mach_symbol->codegen_symbol;
}

LLVMValueRef necro_llvm_intrinsic_get(NecroLLVM* context, NecroMachAstSymbol* mach_symbol, const char* intrinsic_name, LLVMTypeRef fn_type)
{
    NecroLLVMSymbol* symbol = necro_llvm_symbol_get(&context->arena, mach_symbol);
    if (symbol->value == NULL)
    {
        symbol->type  = fn_type;
        symbol->value = LLVMAddFunction(context->mod, intrinsic_name, symbol->type);
        // LLVMSetFunctionCallConv(symbol->value, LLVMFastCallConv);
        // LLVMSetFunctionCallConv(symbol->value, LLVMCCallConv);
    }
    return symbol->value;
}

NecroLLVM necro_llvm_empty()
{
    return (NecroLLVM)
    {
        .arena                   = necro_paged_arena_empty(),
        .snapshot_arena          = necro_snapshot_arena_empty(),
        .intern                  = NULL,
        .base                    = NULL,
        .context                 = NULL,
        .builder                 = NULL,
        .mod                     = NULL,
        .target                  = NULL,
        .target_machine          = NULL,
        .fn_pass_manager         = NULL,
        .mod_pass_manager        = NULL,
        .engine                  = NULL,
        .should_optimize         = false,
        .delayed_phi_node_values = necro_empty_delayed_phi_node_value_vector(),
        .copysign_float          = NULL,
        .copysign_f64            = NULL,
    };
}

NecroLLVM necro_llvm_create(NecroIntern* intern, NecroBase* base, NecroMachProgram* program, bool should_optimize)
{
    LLVMLinkInMCJIT();
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();

    // Context/Mod/Engine
    LLVMContextRef         context          = LLVMContextCreate();
    LLVMModuleRef          mod              = LLVMModuleCreateWithNameInContext("necro", context);
    LLVMExecutionEngineRef engine           = NULL;
    LLVMCodeGenOptLevel    opt_level        = should_optimize ? LLVMCodeGenLevelAggressive : LLVMCodeGenLevelNone;

    // Machine
    const char*   target_triple    = LLVMGetDefaultTargetTriple();
    const char*   target_cpu       = LLVMGetHostCPUName();
    const char*   target_features  = LLVMGetHostCPUFeatures();
    LLVMTargetRef target           = NULL;
    char*         target_error     = NULL;
    if (LLVMGetTargetFromTriple(target_triple, &target, &target_error))
    {
        fprintf(stderr, "necro error: %s\n", target_error);
        LLVMDisposeMessage(target_error);
        necro_exit(1);
        assert(false);
    }
    LLVMTargetMachineRef target_machine = LLVMCreateTargetMachine(target, target_triple, target_cpu, target_features, opt_level, LLVMRelocDefault, LLVMCodeModelJITDefault);
    LLVMTargetDataRef    target_data    = LLVMCreateTargetDataLayout(target_machine);
    LLVMSetTarget(mod, target_triple);
    LLVMSetModuleDataLayout(mod, target_data);

    LLVMPassManagerRef fn_pass_manager  = LLVMCreateFunctionPassManagerForModule(mod);
    LLVMPassManagerRef mod_pass_manager = LLVMCreatePassManager();

    if (should_optimize)
    {
        // LLVMAddDeadStoreEliminationPass(fn_pass_manager);
        // LLVMAddAggressiveDCEPass(fn_pass_manager);
        // LLVMAddTailCallEliminationPass(fn_pass_manager);
        // LLVMAddAggressiveDCEPass(fn_pass_manager);
        // LLVMAddAggressiveInstCombinerPass(mod_pass_manager);

        // fn_pass_manager
        // LLVMAddSimplifyLibCallsPass(fn_pass_manager);
        // LLVMAddPartiallyInlineLibCallsPass(fn_pass_manager);
        LLVMInitializeFunctionPassManager(fn_pass_manager);
        LLVMAddInstructionCombiningPass(fn_pass_manager);
        LLVMAddConstantPropagationPass(fn_pass_manager);
        LLVMAddMergedLoadStoreMotionPass(fn_pass_manager);
        LLVMAddInstructionCombiningPass(fn_pass_manager);
        LLVMAddScalarizerPass(fn_pass_manager);
        LLVMAddScalarReplAggregatesPass(fn_pass_manager);
        LLVMAddScalarReplAggregatesPassSSA(fn_pass_manager);
        LLVMAddScalarizerPass(fn_pass_manager);
        LLVMAddReassociatePass(fn_pass_manager);
        LLVMAddInstructionCombiningPass(fn_pass_manager);
        LLVMAddMergedLoadStoreMotionPass(fn_pass_manager);
        LLVMAddConstantPropagationPass(fn_pass_manager);
        LLVMAddLoopVectorizePass(fn_pass_manager);
        LLVMAddSLPVectorizePass(fn_pass_manager);
        LLVMAddPromoteMemoryToRegisterPass(fn_pass_manager);
        LLVMAddInstructionCombiningPass(fn_pass_manager);
        LLVMFinalizeFunctionPassManager(fn_pass_manager);

        // mod_pass_manager
        LLVMAddTypeBasedAliasAnalysisPass(mod_pass_manager);
        LLVMAddInstructionCombiningPass(mod_pass_manager);
        LLVMAddCFGSimplificationPass(mod_pass_manager);
        LLVMAddConstantPropagationPass(mod_pass_manager);
        LLVMAddMergedLoadStoreMotionPass(mod_pass_manager);
        LLVMAddInstructionCombiningPass(mod_pass_manager);
        LLVMAddArgumentPromotionPass(mod_pass_manager);
        LLVMAddFunctionAttrsPass(mod_pass_manager);
        LLVMAddScalarizerPass(mod_pass_manager);
        LLVMAddScalarReplAggregatesPass(mod_pass_manager);
        LLVMAddScalarReplAggregatesPassSSA(mod_pass_manager);
        LLVMAddScalarizerPass(mod_pass_manager);
        LLVMAddInstructionCombiningPass(mod_pass_manager);
        LLVMAddMergedLoadStoreMotionPass(mod_pass_manager);
        LLVMAddNewGVNPass(mod_pass_manager);
        // LLVMAddGVNPass(mod_pass_manager);
        LLVMAddSCCPPass(mod_pass_manager);
        LLVMAddConstantPropagationPass(mod_pass_manager);
        // LLVMAddLoopVectorizePass(mod_pass_manager);
        // LLVMAddSLPVectorizePass(mod_pass_manager);
        LLVMAddPromoteMemoryToRegisterPass(fn_pass_manager);
        LLVMAddInstructionCombiningPass(mod_pass_manager);
        LLVMAddIPConstantPropagationPass(mod_pass_manager);
        LLVMAddGlobalOptimizerPass(mod_pass_manager);
        LLVMAddGlobalDCEPass(mod_pass_manager);
        LLVMAddDeadStoreEliminationPass(mod_pass_manager);
        // LLVMAddAggressiveDCEPass(mod_pass_manager);
        // LLVMAddBitTrackingDCEPass(mod_pass_manager);
        LLVMAddInstructionCombiningPass(mod_pass_manager);

        // LLVMAddGlobalOptimizerPass(mod_pass_manager);
        LLVMPassManagerBuilderRef pass_manager_builder = LLVMPassManagerBuilderCreate();
        LLVMPassManagerBuilderSetOptLevel(pass_manager_builder, 0);
        // LLVMPassManagerBuilderUseInlinerWithThreshold(pass_manager_builder, 1);
        // LLVMPassManagerBuilderSetOptLevel(pass_manager_builder, opt_level);
        LLVMPassManagerBuilderUseInlinerWithThreshold(pass_manager_builder, 100);
        LLVMPassManagerBuilderPopulateFunctionPassManager(pass_manager_builder, fn_pass_manager);
        LLVMPassManagerBuilderPopulateModulePassManager(pass_manager_builder, mod_pass_manager);
        LLVMPassManagerBuilderDispose(pass_manager_builder);
    }
    else
    {
        // fn_pass_manager
        LLVMInitializeFunctionPassManager(fn_pass_manager);
        // LLVMAddSimplifyLibCallsPass(fn_pass_manager);
        // LLVMAddPartiallyInlineLibCallsPass(fn_pass_manager);
        LLVMFinalizeFunctionPassManager(fn_pass_manager);
        LLVMPassManagerBuilderRef pass_manager_builder = LLVMPassManagerBuilderCreate();
        LLVMPassManagerBuilderSetOptLevel(pass_manager_builder, 0);
        LLVMPassManagerBuilderPopulateFunctionPassManager(pass_manager_builder, fn_pass_manager);
        LLVMPassManagerBuilderPopulateModulePassManager(pass_manager_builder, mod_pass_manager);
        LLVMPassManagerBuilderDispose(pass_manager_builder);
    }
    return (NecroLLVM)
    {
        .arena                   = necro_paged_arena_create(),
        .snapshot_arena          = necro_snapshot_arena_create(),
        .intern                  = intern,
        .base                    = base,
        .context                 = context,
        .builder                 = LLVMCreateBuilderInContext(context),
        .mod                     = mod,
        .target                  = target_data,
        .target_machine          = target_machine,
        .fn_pass_manager         = fn_pass_manager,
        .mod_pass_manager        = mod_pass_manager,
        .should_optimize         = should_optimize,
        .engine                  = engine,
        .program                 = program,
        .delayed_phi_node_values = necro_create_delayed_phi_node_value_vector(),
        .opt_level               = opt_level,
        .copysign_float          = NULL,
        .copysign_f64            = NULL,
    };
}

void necro_llvm_destroy(NecroLLVM* context)
{
    assert(context != NULL);
    if (context->builder != NULL)
        LLVMDisposeBuilder(context->builder);
    if (context->mod_pass_manager != NULL)
        LLVMDisposePassManager(context->mod_pass_manager);
    if (context->fn_pass_manager != NULL)
        LLVMDisposePassManager(context->fn_pass_manager);
    if (context->target != NULL)
        LLVMDisposeTargetData(context->target);
    if (context->target_machine != NULL)
        LLVMDisposeTargetMachine(context->target_machine);
    // if (context->engine != NULL)
    //     LLVMDisposeExecutionEngine(context->engine);
    if (context->mod != NULL) // NOTE: This is else if because it should NOT be destroyed if the engine != NULL, since LLVMDisposeExecutionEngine will also dispose of the module. However if we don't run LLVMDisposeExecutionEngine, then we DO need to dispose of the module...
        LLVMDisposeModule(context->mod);
    if (context->context != NULL)
        LLVMContextDispose(context->context);
    LLVMShutdown();
    necro_destroy_delayed_phi_node_value_vector(&context->delayed_phi_node_values);
    necro_paged_arena_destroy(&context->arena);
    necro_snapshot_arena_destroy(&context->snapshot_arena);
    *context = necro_llvm_empty();
}


///////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////
void necro_llvm_print(NecroLLVM* context)
{
    fflush(stdout);
    fflush(stderr);
    LLVMDumpModule(context->mod);
}

void necro_llvm_verify_and_dump(NecroLLVM* context)
{
    char* error = NULL;
    // LLVMVerifyModule(context->mod, LLVMAbortProcessAction, &error);
    LLVMVerifyModule(context->mod, LLVMPrintMessageAction, &error);
    if (strlen(error) != 0)
    {
        LLVMDumpModule(context->mod);
        // necro_llvm_print(context);
        fprintf(stderr, "\n\n");
        fprintf(stderr, "LLVM error: %s\n", error);
        LLVMDisposeMessage(error);
        // return NECRO_ERROR;
        exit(1);
        return;
    }
    LLVMDisposeMessage(error);
    // return NECRO_SUCCESS;
    return;
}

LLVMTypeRef necro_llvm_type_from_mach_type(NecroLLVM* context, NecroMachType* mach_type)
{
    assert(mach_type != NULL);
    switch (mach_type->type)
    {
    case NECRO_MACH_TYPE_UINT1:  return LLVMInt1TypeInContext(context->context);
    case NECRO_MACH_TYPE_UINT8:  return LLVMInt8TypeInContext(context->context);
    case NECRO_MACH_TYPE_UINT16: return LLVMInt16TypeInContext(context->context);
    case NECRO_MACH_TYPE_UINT32: return LLVMInt32TypeInContext(context->context);
    case NECRO_MACH_TYPE_UINT64: return LLVMInt64TypeInContext(context->context);
    case NECRO_MACH_TYPE_INT32:  return LLVMInt32TypeInContext(context->context);
    case NECRO_MACH_TYPE_INT64:  return LLVMInt64TypeInContext(context->context);
    case NECRO_MACH_TYPE_F32:    return LLVMFloatTypeInContext(context->context);
    case NECRO_MACH_TYPE_F64:    return LLVMDoubleTypeInContext(context->context);
    case NECRO_MACH_TYPE_VOID:   return LLVMVoidTypeInContext(context->context);
    case NECRO_MACH_TYPE_PTR:    return LLVMPointerType(necro_llvm_type_from_mach_type(context, mach_type->ptr_type.element_type), 0);
    case NECRO_MACH_TYPE_ARRAY:  return LLVMArrayType(necro_llvm_type_from_mach_type(context, mach_type->array_type.element_type), (unsigned int) mach_type->array_type.element_count);
    case NECRO_MACH_TYPE_STRUCT:
    {
        NecroLLVMSymbol* llvm_symbol = necro_llvm_symbol_get(&context->arena, mach_type->struct_type.symbol);
        if (llvm_symbol->type == NULL)
        {
            LLVMTypeRef  struct_type = LLVMStructCreateNamed(context->context, mach_type->struct_type.symbol->name->str);
            llvm_symbol->type        = struct_type;
            LLVMTypeRef* elements    = necro_paged_arena_alloc(&context->arena, mach_type->struct_type.num_members * sizeof(LLVMTypeRef));
            for (size_t i = 0; i < mach_type->struct_type.num_members; ++i)
            {
                elements[i] = necro_llvm_type_from_mach_type(context, mach_type->struct_type.members[i]);
            }
            LLVMStructSetBody(struct_type, elements, (unsigned int) mach_type->struct_type.num_members, false);
        }
        return llvm_symbol->type;
    }
    case NECRO_MACH_TYPE_FN:
    {
        LLVMTypeRef  return_type = necro_llvm_type_from_mach_type(context, mach_type->fn_type.return_type);
        LLVMTypeRef* parameters  = necro_paged_arena_alloc(&context->arena, mach_type->fn_type.num_parameters * sizeof(LLVMTypeRef));
        for (size_t i = 0; i < mach_type->fn_type.num_parameters; ++i)
        {
            parameters[i] = necro_llvm_type_from_mach_type(context, mach_type->fn_type.parameters[i]);
        }
        return LLVMFunctionType(return_type, parameters, (unsigned int) mach_type->fn_type.num_parameters, false);
    }
    default:
        assert(false);
        return NULL;
    }
}

bool necro_llvm_type_is_unboxed(NecroLLVM* context, LLVMTypeRef type)
{
    return type == necro_llvm_type_from_mach_type(context, context->program->type_cache.word_int_type)
        || type == necro_llvm_type_from_mach_type(context, context->program->type_cache.word_uint_type)
        || type == necro_llvm_type_from_mach_type(context, context->program->type_cache.word_float_type);
}

///////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////
LLVMValueRef necro_llvm_codegen_call(NecroLLVM* context, NecroMachAst* ast);
LLVMValueRef necro_llvm_codegen_call_intrinsic(NecroLLVM* context, NecroMachAst* ast);
void necro_llvm_set_intrinsic_uop_type_and_value(NecroLLVM* context, NecroMachType* arg_mach_type, NecroAstSymbol* symbol_32, NecroAstSymbol* symbol_64, const char* name_32, const char* name_64, LLVMTypeRef cmp_type_32, LLVMTypeRef* fn_type, LLVMValueRef* fn_value);

///////////////////////////////////////////////////////
// NecroDelayedPhiNodeValue
///////////////////////////////////////////////////////
void necro_llvm_codegen_delayed_phi_node(NecroLLVM* context, NecroLLVMSymbol* value)
{
    size_t i = 0;
    while (i < context->delayed_phi_node_values.length)
    {
        if (value->mach_symbol->name == context->delayed_phi_node_values.data[i].value_name)
        {
            // Add Phi Node
            LLVMValueRef      leaf_value             = value->value;
            LLVMBasicBlockRef block                  = context->delayed_phi_node_values.data[i].block;
            LLVMValueRef      phi_value              = context->delayed_phi_node_values.data[i].phi_node;
            LLVMAddIncoming(phi_value, &leaf_value, &block, 1);
            // Swap with end and Pop off
            context->delayed_phi_node_values.data[i] = context->delayed_phi_node_values.data[context->delayed_phi_node_values.length - 1];
            necro_pop_delayed_phi_node_value_vector(&context->delayed_phi_node_values);
        }
        else
        {
            i++;
        }
    }
}

void necro_llvm_add_delayed_phi_node(NecroLLVM* context, NecroMachAst* value, LLVMBasicBlockRef block, LLVMValueRef phi_node)
{
    assert(value->type == NECRO_MACH_VALUE);
    assert(value->value.value_type == NECRO_MACH_VALUE_REG);
    NecroDelayedPhiNodeValue delayed_phi_node = (NecroDelayedPhiNodeValue) { .value_name = value->value.reg_symbol->name, .block = block, .phi_node = phi_node };
    necro_push_delayed_phi_node_value_vector(&context->delayed_phi_node_values, &delayed_phi_node);
}

///////////////////////////////////////////////////////
// Codegen
///////////////////////////////////////////////////////
LLVMValueRef necro_llvm_codegen_value(NecroLLVM* context, NecroMachAst* ast)
{
    assert(context != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACH_VALUE);
    switch (ast->value.value_type)
    {
    case NECRO_MACH_VALUE_UNDEFINED:        return LLVMGetUndef(necro_llvm_type_from_mach_type(context, ast->necro_machine_type));
    case NECRO_MACH_VALUE_GLOBAL:           return necro_llvm_symbol_get(&context->arena, ast->value.global_symbol)->value;
    case NECRO_MACH_VALUE_REG:              return necro_llvm_symbol_get(&context->arena, ast->value.reg_symbol)->value;
    case NECRO_MACH_VALUE_PARAM:            return LLVMGetParam(necro_llvm_symbol_get(&context->arena, ast->value.param_reg.fn_symbol)->value, (unsigned int) ast->value.param_reg.param_num);
    case NECRO_MACH_VALUE_UINT1_LITERAL:    return LLVMConstInt(LLVMInt1TypeInContext(context->context),  ast->value.uint1_literal,  false);
    case NECRO_MACH_VALUE_UINT8_LITERAL:    return LLVMConstInt(LLVMInt8TypeInContext(context->context),  ast->value.uint8_literal,  false);
    case NECRO_MACH_VALUE_UINT16_LITERAL:   return LLVMConstInt(LLVMInt16TypeInContext(context->context), ast->value.uint16_literal, false);
    case NECRO_MACH_VALUE_UINT32_LITERAL:   return LLVMConstInt(LLVMInt32TypeInContext(context->context), ast->value.uint32_literal, false);
    case NECRO_MACH_VALUE_UINT64_LITERAL:   return LLVMConstInt(LLVMInt64TypeInContext(context->context), ast->value.uint64_literal, false);
    case NECRO_MACH_VALUE_INT32_LITERAL:    return LLVMConstInt(LLVMInt32TypeInContext(context->context), ast->value.int32_literal,  true);
    case NECRO_MACH_VALUE_INT64_LITERAL:    return LLVMConstInt(LLVMInt64TypeInContext(context->context), ast->value.int64_literal,  true);
    case NECRO_MACH_VALUE_F32_LITERAL:      return LLVMConstReal(LLVMFloatTypeInContext(context->context), ast->value.f32_literal);
    case NECRO_MACH_VALUE_F64_LITERAL:      return LLVMConstReal(LLVMDoubleTypeInContext(context->context), ast->value.f64_literal);
    case NECRO_MACH_VALUE_NULL_PTR_LITERAL: return LLVMConstPointerNull(necro_llvm_type_from_mach_type(context, ast->necro_machine_type));
    default:                 assert(false); return NULL;
    }
}

LLVMValueRef necro_llvm_codegen_store(NecroLLVM* context, NecroMachAst* ast)
{
    assert(context != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACH_STORE);
    LLVMValueRef source_value = necro_llvm_codegen_value(context, ast->store.source_value);
    LLVMValueRef dest_ptr     = necro_llvm_codegen_value(context, ast->store.dest_ptr);
    return LLVMBuildStore(context->builder, source_value, dest_ptr);
}

LLVMValueRef necro_llvm_codegen_load(NecroLLVM* context, NecroMachAst* ast)
{
    assert(context != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACH_LOAD);
    LLVMValueRef     source_ptr = necro_llvm_codegen_value(context, ast->load.source_ptr);
    const char*      dest_name  = ast->load.dest_value->value.reg_symbol->name->str;
    LLVMValueRef     result     = LLVMBuildLoad(context->builder, source_ptr, dest_name);
    NecroLLVMSymbol* symbol     = necro_llvm_symbol_get(&context->arena, ast->load.dest_value->value.reg_symbol);
    symbol->value               = result;
    necro_llvm_codegen_delayed_phi_node(context, symbol);
    return result;
}

LLVMValueRef necro_llvm_codegen_zext(NecroLLVM* context, NecroMachAst* ast)
{
    assert(context != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACH_ZEXT);
    LLVMValueRef     value  = LLVMBuildZExt(context->builder, necro_llvm_codegen_value(context, ast->zext.from_value), necro_llvm_type_from_mach_type(context, ast->zext.to_value->necro_machine_type), "zxt");
    NecroLLVMSymbol* symbol = necro_llvm_symbol_get(&context->arena, ast->zext.to_value->value.reg_symbol);
    symbol->type            = LLVMTypeOf(value);
    symbol->value           = value;
    necro_llvm_codegen_delayed_phi_node(context, symbol);
    return value;
}

LLVMValueRef necro_llvm_codegen_gep(NecroLLVM* context, NecroMachAst* ast)
{
    assert(context != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACH_GEP);
    // NecroArenaSnapshot snapshot = necro_snapshot_arena_get(&context->snapshot_arena);
    const char*        name     = ast->gep.dest_value->value.reg_symbol->name->str;
    LLVMValueRef       ptr      = necro_llvm_codegen_value(context, ast->gep.source_value);
    // LLVMValueRef*      indices  = necro_snapshot_arena_alloc(&context->snapshot_arena, ast->gep.num_indices * sizeof(LLVMValueRef));
    LLVMValueRef*      indices  = necro_paged_arena_alloc(&context->arena, ast->gep.num_indices * sizeof(LLVMValueRef));
    for (size_t i = 0; i < ast->gep.num_indices; ++i)
    {
        NecroMachAst* index = ast->gep.indices[i];
        indices[i]          = necro_llvm_codegen_value(context, index);
        indices[i]          = LLVMBuildTrunc(context->builder, indices[i], LLVMInt32TypeInContext(context->context), "ix");
        assert(indices[i] != NULL);
    }
    LLVMValueRef     value  = LLVMBuildGEP(context->builder, ptr, indices, (unsigned int) ast->gep.num_indices, name);
    NecroLLVMSymbol* symbol = necro_llvm_symbol_get(&context->arena, ast->gep.dest_value->value.reg_symbol);
    symbol->type            = necro_llvm_type_from_mach_type(context, ast->gep.dest_value->necro_machine_type);
    symbol->value           = value;
    necro_llvm_codegen_delayed_phi_node(context, symbol);
    // necro_snapshot_arena_rewind(&context->snapshot_arena, snapshot);
    return value;
}

LLVMValueRef necro_llvm_codegen_insert_value(NecroLLVM* context, NecroMachAst* ast)
{
    assert(context != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACH_INSERT_VALUE);
    const char*      name            = ast->insert_value.dest_value->value.reg_symbol->name->str;
    LLVMValueRef     aggregate_value = necro_llvm_codegen_value(context, ast->insert_value.aggregate_value);
    LLVMValueRef     inserted_value  = necro_llvm_codegen_value(context, ast->insert_value.inserted_value);
    LLVMValueRef     value           = LLVMBuildInsertValue(context->builder, aggregate_value, inserted_value, (unsigned int) ast->insert_value.index, name);
    NecroLLVMSymbol* symbol          = necro_llvm_symbol_get(&context->arena, ast->insert_value.dest_value->value.reg_symbol);
    symbol->type                     = necro_llvm_type_from_mach_type(context, ast->insert_value.dest_value->necro_machine_type);
    symbol->value                    = value;
    necro_llvm_codegen_delayed_phi_node(context, symbol);
    return value;
}

LLVMValueRef necro_llvm_codegen_extract_value(NecroLLVM* context, NecroMachAst* ast)
{
    assert(context != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACH_EXTRACT_VALUE);
    const char*      name            = ast->extract_value.dest_value->value.reg_symbol->name->str;
    LLVMValueRef     aggregate_value = necro_llvm_codegen_value(context, ast->extract_value.aggregate_value);
    LLVMValueRef     value           = LLVMBuildExtractValue(context->builder, aggregate_value, (unsigned int) ast->extract_value.index, name);
    NecroLLVMSymbol* symbol          = necro_llvm_symbol_get(&context->arena, ast->extract_value.dest_value->value.reg_symbol);
    symbol->type                     = necro_llvm_type_from_mach_type(context, ast->extract_value.dest_value->necro_machine_type);
    symbol->value                    = value;
    necro_llvm_codegen_delayed_phi_node(context, symbol);
    return value;
}


#define NECRO_CODEGEN_FLOAT_X_FLOAT_BIT_BINOP(BIT_BINOP)\
{\
    LLVMTypeRef arg_type = necro_llvm_type_from_mach_type(context, ast->binop.left->necro_machine_type);\
    assert(arg_type != NULL);\
    const LLVMTypeRef f32_type = necro_llvm_type_from_mach_type(context, context->program->type_cache.f32_type);\
    assert(f32_type != NULL);\
    const LLVMTypeRef f64_type = necro_llvm_type_from_mach_type(context, context->program->type_cache.f64_type);\
    assert(f64_type != NULL);\
    if (arg_type == f32_type)\
    {\
        const LLVMTypeRef uint32_type = necro_llvm_type_from_mach_type(context, context->program->type_cache.uint32_type);\
        assert(uint32_type != NULL);\
        LLVMValueRef lhs = LLVMBuildBitCast(context->builder, left, uint32_type, "lhs_float_to_uint");\
        LLVMValueRef rhs = LLVMBuildBitCast(context->builder, right, uint32_type, "rhs_float_to_uint");\
        value = BIT_BINOP(context->builder, lhs, rhs, name);\
        value = LLVMBuildBitCast(context->builder, value, f32_type, "uint_to_float");\
    }\
    else if (arg_type == f64_type)\
    {\
        const LLVMTypeRef uint64_type = necro_llvm_type_from_mach_type(context, context->program->type_cache.uint64_type);\
        assert(uint64_type != NULL);\
        LLVMValueRef lhs = LLVMBuildBitCast(context->builder, left, uint64_type, "lhs_float64_to_uint64");\
        LLVMValueRef rhs = LLVMBuildBitCast(context->builder, right, uint64_type, "rhs_float64_to_uint64");\
        value = BIT_BINOP(context->builder, lhs, rhs, name);\
        value = LLVMBuildBitCast(context->builder, value, f64_type, "uint64_to_float64");\
    }\
    else\
    {\
        assert(false && "Only 32-bit and 64-bit Floats supported");\
    }\
}

#define NECRO_CODEGEN_FLOAT_X_UINT_BIT_BINOP(BIT_BINOP)\
{\
    LLVMTypeRef arg_type = necro_llvm_type_from_mach_type(context, ast->binop.left->necro_machine_type);\
    assert(arg_type != NULL);\
    const LLVMTypeRef f32_type = necro_llvm_type_from_mach_type(context, context->program->type_cache.f32_type);\
    assert(f32_type != NULL);\
    const LLVMTypeRef f64_type = necro_llvm_type_from_mach_type(context, context->program->type_cache.f64_type);\
    assert(f64_type != NULL);\
    if (arg_type == f32_type)\
    {\
        const LLVMTypeRef uint32_type = necro_llvm_type_from_mach_type(context, context->program->type_cache.uint32_type);\
        assert(uint32_type != NULL);\
        value = LLVMBuildBitCast(context->builder, left, uint32_type, "float_to_uint");\
        value = BIT_BINOP(context->builder, value, right, name);\
        value = LLVMBuildBitCast(context->builder, value, f32_type, "uint_to_float");\
    }\
    else if (arg_type == f64_type)\
    {\
        const LLVMTypeRef uint64_type = necro_llvm_type_from_mach_type(context, context->program->type_cache.uint64_type);\
        assert(uint64_type != NULL);\
        value = LLVMBuildBitCast(context->builder, left, uint64_type, "float64_to_uint64");\
        value = BIT_BINOP(context->builder, value, right, name);\
        value = LLVMBuildBitCast(context->builder, value, f64_type, "uint64_to_float64");\
    }\
    else\
    {\
        assert(false && "Only 32-bit and 64-bit Floats supported");\
    }\
}

LLVMValueRef necro_llvm_codegen_binop(NecroLLVM* context, NecroMachAst* ast)
{
    assert(context != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACH_BINOP);
    const char*  name  = ast->binop.result->value.reg_symbol->name->str;
    LLVMValueRef value = NULL;
    LLVMValueRef left  = necro_llvm_codegen_value(context, ast->binop.left);
    LLVMValueRef right = necro_llvm_codegen_value(context, ast->binop.right);
    switch (ast->binop.binop_type)
    {
    case NECRO_PRIMOP_BINOP_IADD: value = LLVMBuildAdd(context->builder, left, right, name);  break;
    case NECRO_PRIMOP_BINOP_ISUB: value = LLVMBuildSub(context->builder, left, right, name);  break;
    case NECRO_PRIMOP_BINOP_IMUL: value = LLVMBuildMul(context->builder, left, right, name);  break;
    case NECRO_PRIMOP_BINOP_IDIV: value = LLVMBuildSDiv(context->builder, left, right, name); break;
    case NECRO_PRIMOP_BINOP_IREM: value = LLVMBuildSRem(context->builder, left, right, name); break;
    case NECRO_PRIMOP_BINOP_UADD: value = LLVMBuildAdd(context->builder, left, right, name);  break;
    case NECRO_PRIMOP_BINOP_USUB: value = LLVMBuildSub(context->builder, left, right, name);  break;
    case NECRO_PRIMOP_BINOP_UMUL: value = LLVMBuildMul(context->builder, left, right, name);  break;
    case NECRO_PRIMOP_BINOP_UDIV: value = LLVMBuildUDiv(context->builder, left, right, name); break;
    case NECRO_PRIMOP_BINOP_UREM: value = LLVMBuildURem(context->builder, left, right, name); break;
    case NECRO_PRIMOP_BINOP_FADD: value = LLVMBuildFAdd(context->builder, left, right, name); break;
    case NECRO_PRIMOP_BINOP_FSUB: value = LLVMBuildFSub(context->builder, left, right, name); break;
    case NECRO_PRIMOP_BINOP_FMUL: value = LLVMBuildFMul(context->builder, left, right, name); break;
    case NECRO_PRIMOP_BINOP_FDIV: value = LLVMBuildFDiv(context->builder, left, right, name); break;
    case NECRO_PRIMOP_BINOP_FREM: value = LLVMBuildFRem(context->builder, left, right, name); break;
    case NECRO_PRIMOP_BINOP_AND:  value = LLVMBuildAnd(context->builder, left, right, name);  break;
    case NECRO_PRIMOP_BINOP_OR:   value = LLVMBuildOr(context->builder, left, right, name);   break;
    case NECRO_PRIMOP_BINOP_XOR:  value = LLVMBuildXor(context->builder, left, right, name);  break;
    case NECRO_PRIMOP_BINOP_SHL:  value = LLVMBuildShl(context->builder, left, right, name);  break;
    case NECRO_PRIMOP_BINOP_SHR:  value = LLVMBuildLShr(context->builder, left, right, name); break;
    case NECRO_PRIMOP_BINOP_FAND:
      NECRO_CODEGEN_FLOAT_X_FLOAT_BIT_BINOP(LLVMBuildAnd);
      break;
    case NECRO_PRIMOP_BINOP_FOR:
      NECRO_CODEGEN_FLOAT_X_FLOAT_BIT_BINOP(LLVMBuildOr);
      break;
    case NECRO_PRIMOP_BINOP_FXOR:
      NECRO_CODEGEN_FLOAT_X_FLOAT_BIT_BINOP(LLVMBuildXor);
      break;
    case NECRO_PRIMOP_BINOP_FSHL:
      NECRO_CODEGEN_FLOAT_X_UINT_BIT_BINOP(LLVMBuildShl);
      break;
    case NECRO_PRIMOP_BINOP_FSHR:
      NECRO_CODEGEN_FLOAT_X_UINT_BIT_BINOP(LLVMBuildLShr);
      break;

    default:
        assert(false);
        break;
    }
    NecroLLVMSymbol* symbol = necro_llvm_symbol_get(&context->arena, ast->binop.result->value.reg_symbol);
    symbol->type            = necro_llvm_type_from_mach_type(context, ast->binop.result->necro_machine_type);
    symbol->value           = value;
    necro_llvm_codegen_delayed_phi_node(context, symbol);
    return value;
}

// TODO: FINISH!
LLVMValueRef necro_llvm_codegen_uop(NecroLLVM* context, NecroMachAst* ast)
{
    assert(context != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACH_UOP);
    const char*  name        = ast->uop.result->value.reg_symbol->name->str;
    LLVMValueRef value       = NULL;
    LLVMValueRef param       = necro_llvm_codegen_value(context, ast->uop.param);
    LLVMTypeRef  result_type = necro_llvm_type_from_mach_type(context, ast->uop.result->necro_machine_type);
    // TODO: Implement Abs and sign
    switch (ast->uop.uop_type)
    {
    case NECRO_PRIMOP_UOP_UABS: value = param; break;
    case NECRO_PRIMOP_UOP_IABS:
    {
        // Note: This assume's Two's complement representation for negative integers!
        // In 32-bit, this looks like:
        // abs x = if x >= 0 then (x XOR 0x0) + 0 else (x XOR 0xFFFFFFFF) + 1
        LLVMTypeRef  arg_type  = necro_llvm_type_from_mach_type(context, ast->uop.param->necro_machine_type);
        LLVMValueRef xor_mask  = NULL;
        LLVMValueRef add_value = NULL;
        value                  = param;
        if (arg_type == LLVMInt32TypeInContext(context->context))
        {
            xor_mask  = LLVMBuildAShr(context->builder, value, LLVMConstInt(LLVMInt32TypeInContext(context->context), 31, false), "xor_mask");
            add_value = LLVMBuildLShr(context->builder, value, LLVMConstInt(LLVMInt32TypeInContext(context->context), 31, false), "sub_value");
        }
        else if (arg_type == LLVMInt64TypeInContext(context->context))
        {
            xor_mask  = LLVMBuildAShr(context->builder, value, LLVMConstInt(LLVMInt64TypeInContext(context->context), 63, false), "xor_mask");
            add_value = LLVMBuildLShr(context->builder, value, LLVMConstInt(LLVMInt64TypeInContext(context->context), 63, false), "sub_value");
        }
        else
            assert(false && "Only 32-bit and 64-bit Ints supported");
        value = LLVMBuildXor(context->builder, value, xor_mask, "xor_result");
        value = LLVMBuildAdd(context->builder, value, add_value, "abs_result");
        break;
    }

    case NECRO_PRIMOP_UOP_USGN:
    {
        LLVMTypeRef arg_type = necro_llvm_type_from_mach_type(context, ast->uop.param->necro_machine_type);
        if (arg_type == LLVMInt32TypeInContext(context->context))
            value = LLVMConstInt(LLVMInt32TypeInContext(context->context), 1, false);
        else if (arg_type == LLVMInt64TypeInContext(context->context))
            value = LLVMConstInt(LLVMInt64TypeInContext(context->context), 1, false);
        else
            assert(false && "Only 32-bit and 64-bit Ints supported");
        break;
    }
    case NECRO_PRIMOP_UOP_ISGN:
    {
        LLVMTypeRef  arg_type = necro_llvm_type_from_mach_type(context, ast->uop.param->necro_machine_type);
        if (arg_type == LLVMInt32TypeInContext(context->context))
        {
            value = LLVMBuildAShr(context->builder, param, LLVMConstInt(LLVMInt32TypeInContext(context->context), 32, false), "ashr_value");
            value = LLVMBuildOr(context->builder, value, LLVMConstInt(LLVMInt32TypeInContext(context->context), 1, false), "sign_value");
        }
        else if (arg_type == LLVMInt64TypeInContext(context->context))
        {
            value = LLVMBuildAShr(context->builder, param, LLVMConstInt(LLVMInt64TypeInContext(context->context), 63, false), "ashr_value");
            value = LLVMBuildOr(context->builder, value, LLVMConstInt(LLVMInt64TypeInContext(context->context), 1, true), "sign_value");
        }
        else
        {
            assert(false && "Only 32-bit and 64-bit Ints supported");
        }
        break;
    }
    case NECRO_PRIMOP_UOP_FSGN:
    {
        LLVMTypeRef arg_type = necro_llvm_type_from_mach_type(context, ast->uop.param->necro_machine_type);
        if (arg_type == LLVMFloatTypeInContext(context->context))
        {
            if (context->copysign_float == NULL)
            {
                LLVMTypeRef copysign_float_type = LLVMFunctionType(LLVMFloatTypeInContext(context->context), (LLVMTypeRef[]) { LLVMFloatTypeInContext(context->context), LLVMFloatTypeInContext(context->context) }, 2, false);
                context->copysign_float         = necro_llvm_symbol_create(&context->arena, copysign_float_type, LLVMAddFunction(context->mod, "llvm.copysign.f32", copysign_float_type), NULL, NULL);
            }
            value = LLVMBuildCall(context->builder, context->copysign_float->value, (LLVMValueRef[]) { LLVMConstReal(LLVMFloatTypeInContext(context->context), 1), param }, 2, "sign_value");
            LLVMSetInstructionCallConv(value, LLVMGetFunctionCallConv(context->copysign_float->value));
        }
        else if (arg_type == LLVMDoubleTypeInContext(context->context))
        {
            if (context->copysign_f64 == NULL)
            {
                LLVMTypeRef copysign_f64_type = LLVMFunctionType(LLVMDoubleTypeInContext(context->context), (LLVMTypeRef[]) { LLVMDoubleTypeInContext(context->context), LLVMDoubleTypeInContext(context->context) }, 2, false);
                context->copysign_f64         = necro_llvm_symbol_create(&context->arena, copysign_f64_type, LLVMAddFunction(context->mod, "llvm.copysign.f64", copysign_f64_type), NULL, NULL);
            }
            value = LLVMBuildCall(context->builder, context->copysign_f64->value, (LLVMValueRef[]) { LLVMConstReal(LLVMDoubleTypeInContext(context->context), 1), param }, 2, "sign_value");
            LLVMSetInstructionCallConv(value, LLVMGetFunctionCallConv(context->copysign_f64->value));
        }
        else
        {
            assert(false && "Only 32-bit and 64-bit Floats supported");
        }
        break;
    }

    case NECRO_PRIMOP_UOP_FBREV:
    {
        /*
           %1 = bitcast double %0 to i64
           %2 = bitreverse i64 %1
           %3 = bitcast i64 %2 to double
           ret double %3
        */

        const LLVMTypeRef arg_type = necro_llvm_type_from_mach_type(context, ast->uop.param->necro_machine_type);
        assert(arg_type != NULL);
        const LLVMTypeRef uint32_type = necro_llvm_type_from_mach_type(context, context->program->type_cache.uint32_type);
        assert(uint32_type != NULL);
        const LLVMTypeRef f32_type = necro_llvm_type_from_mach_type(context, context->program->type_cache.f32_type);
        assert(f32_type != NULL);
        const LLVMTypeRef uint64_type = necro_llvm_type_from_mach_type(context, context->program->type_cache.uint64_type);
        assert(uint64_type != NULL);
        const LLVMTypeRef f64_type = necro_llvm_type_from_mach_type(context, context->program->type_cache.f64_type);
        assert(f64_type != NULL);

        LLVMTypeRef bitrev_type = NULL;
        LLVMValueRef fn_value   = NULL;

        if (arg_type == f32_type)
        {
            assert(context->base->bit_reverse_float != NULL);
            value = LLVMBuildBitCast(context->builder, param, uint32_type, "float_to_uint");
            necro_llvm_set_intrinsic_uop_type_and_value(
                context,
                context->program->type_cache.uint32_type,
                context->base->bit_reverse_float,
                context->base->bit_reverse_float,
                "llvm.bitreverse.i32",
                "llvm.bitreverse.i64",
                uint32_type,
                &bitrev_type,
                &fn_value);

            LLVMValueRef* params = necro_paged_arena_alloc(&context->arena, sizeof(LLVMValueRef));
            *params = value;

            value = LLVMBuildCall(context->builder, fn_value, params, (unsigned int) 1, "call_bitreverse");
            LLVMSetInstructionCallConv(value, LLVMGetFunctionCallConv(fn_value));
            value = LLVMBuildBitCast(context->builder, value, f32_type, "uint_to_float");
        }
        else if (arg_type == f64_type)
        {
            NecroAstSymbol* bit_reverse_float = context->base->bit_reverse_float;
            if (bit_reverse_float == NULL ||
                bit_reverse_float->core_ast_symbol == NULL ||
                bit_reverse_float->core_ast_symbol->mach_symbol == NULL)
            {
                bit_reverse_float = context->base->bit_reverse_float64;
            }
            assert(bit_reverse_float != NULL);
            assert(bit_reverse_float->core_ast_symbol->mach_symbol != NULL);
            value = LLVMBuildBitCast(context->builder, param, uint64_type, "float64_to_uint64");
            necro_llvm_set_intrinsic_uop_type_and_value(
                context,
                context->program->type_cache.uint64_type,
                bit_reverse_float,
                bit_reverse_float,
                "llvm.bitreverse.i32",
                "llvm.bitreverse.i64",
                uint32_type,
                &bitrev_type,
                &fn_value);

            LLVMValueRef* params = necro_paged_arena_alloc(&context->arena, sizeof(LLVMValueRef));
            *params = value;

            value = LLVMBuildCall(context->builder, fn_value, params, (unsigned int) 1, "call_bitreverse");
            LLVMSetInstructionCallConv(value, LLVMGetFunctionCallConv(fn_value));
            value = LLVMBuildBitCast(context->builder, value, f64_type, "uint64_to_float64");
        }
        else
        {
            assert(false && "Only 32-bit and 64-bit Floats supported");
        }
        break;
    }

	case NECRO_PRIMOP_UOP_NOT:  value = LLVMBuildNot(context->builder, param, name);  break;
	case NECRO_PRIMOP_UOP_FNOT:
	{
		/*
			%1 = bitcast double %0 to i64
			%2 = not i64 %1
			%3 = bitcast i64 %2 to double
			ret double %3
		*/

		const LLVMTypeRef arg_type = necro_llvm_type_from_mach_type(context, ast->uop.param->necro_machine_type);
		assert(arg_type != NULL);
		const LLVMTypeRef f32_type = necro_llvm_type_from_mach_type(context, context->program->type_cache.f32_type);
		assert(f32_type != NULL);
		const LLVMTypeRef f64_type = necro_llvm_type_from_mach_type(context, context->program->type_cache.f64_type);
		assert(f64_type != NULL);
		if (arg_type == f32_type)
		{
			const LLVMTypeRef uint32_type = necro_llvm_type_from_mach_type(context, context->program->type_cache.uint32_type);
			assert(uint32_type != NULL);
			value = LLVMBuildBitCast(context->builder, param, uint32_type, "float_to_uint");
			value = LLVMBuildNot(context->builder, value, name);
			value = LLVMBuildBitCast(context->builder, value, f32_type, "uint_to_float");
		}
		else if (arg_type == f64_type)
		{
			const LLVMTypeRef uint64_type = necro_llvm_type_from_mach_type(context, context->program->type_cache.uint64_type);
			assert(uint64_type != NULL);
			value = LLVMBuildBitCast(context->builder, param, uint64_type, "float64_to_uint64");
			value = LLVMBuildNot(context->builder, value, name);
			value = LLVMBuildBitCast(context->builder, value, f64_type, "uint64_to_float64");
		}
        else
        {
            assert(false && "Only 32-bit and 64-bit Floats supported");
        }
		break;
	}

    case NECRO_PRIMOP_UOP_FTOB:
    {
        /*
           %1 = bitcast double %0 to i64
           ret i64 %1
        */

        const LLVMTypeRef arg_type = necro_llvm_type_from_mach_type(context, ast->uop.param->necro_machine_type);
        assert(arg_type != NULL);
        const LLVMTypeRef f32_type = necro_llvm_type_from_mach_type(context, context->program->type_cache.f32_type);
        assert(f32_type != NULL);
        const LLVMTypeRef f64_type = necro_llvm_type_from_mach_type(context, context->program->type_cache.f64_type);
        assert(f64_type != NULL);

        if (arg_type == f32_type)
        {
			const LLVMTypeRef uint32_type = necro_llvm_type_from_mach_type(context, context->program->type_cache.uint32_type);
			assert(uint32_type != NULL);
            value = LLVMBuildBitCast(context->builder, param, uint32_type, "float_to_uint");
        }
        else if (arg_type == f64_type)
        {
			const LLVMTypeRef uint64_type = necro_llvm_type_from_mach_type(context, context->program->type_cache.uint64_type);
			assert(uint64_type != NULL);
            value = LLVMBuildBitCast(context->builder, param, uint64_type, "float64_to_uint64");
        }
        else
        {
            assert(false && "Only 32-bit and 64-bit Floats supported");
        }
        break;
    }

    case NECRO_PRIMOP_UOP_FFRB:
    {
        /*
           %1 = bitcast i64 %0 to double
           ret double %1
        */

        const LLVMTypeRef arg_type = necro_llvm_type_from_mach_type(context, ast->uop.param->necro_machine_type);
        assert(arg_type != NULL);
		const LLVMTypeRef uint32_type = necro_llvm_type_from_mach_type(context, context->program->type_cache.uint32_type);
		assert(uint32_type != NULL);
		const LLVMTypeRef uint64_type = necro_llvm_type_from_mach_type(context, context->program->type_cache.uint64_type);
		assert(uint64_type != NULL);

        if (arg_type == uint32_type)
        {
			const LLVMTypeRef f32_type = necro_llvm_type_from_mach_type(context, context->program->type_cache.f32_type);
			assert(f32_type != NULL);
            value = LLVMBuildBitCast(context->builder, param, f32_type, "uint_to_float");
        }
        else if (arg_type == uint64_type)
        {
			const LLVMTypeRef f64_type = necro_llvm_type_from_mach_type(context, context->program->type_cache.f64_type);
			assert(f64_type != NULL);
        	value = LLVMBuildBitCast(context->builder, param, f64_type, "uint64_to_float64");
        }
        else
        {
            assert(false && "Only 32-bit and 64-bit Floats supported");
        }
        break;
    }

    case NECRO_PRIMOP_UOP_ITOU: value = param; break; // TODO: Different bit sizes?
    case NECRO_PRIMOP_UOP_ITOF: value = LLVMBuildSIToFP(context->builder, param, result_type, name); break;
    case NECRO_PRIMOP_UOP_ITOI:
    {
        const size_t param_size  = necro_mach_type_calculate_size_in_bytes(context->program, ast->uop.param->necro_machine_type);
        const size_t result_size = necro_mach_type_calculate_size_in_bytes(context->program, ast->uop.result->necro_machine_type);
        if (param_size < result_size)
            value = LLVMBuildSExt(context->builder, param, result_type, name);
        else if (param_size > result_size)
            value = LLVMBuildTrunc(context->builder, param, result_type, name);
        else
            value = param;
        break;
    }

    case NECRO_PRIMOP_UOP_UTOI: value = param; break; // TODO: Different bit sizes?

    case NECRO_PRIMOP_UOP_FTRI: value = LLVMBuildFPToSI(context->builder, param, result_type, name); break; // TODO: Finish
    case NECRO_PRIMOP_UOP_FTRU: value = LLVMBuildFPToUI(context->builder, param, result_type, name); break; // TODO: Finish
    case NECRO_PRIMOP_UOP_FRNI: value = LLVMBuildFPToSI(context->builder, param, result_type, name); break;
    case NECRO_PRIMOP_UOP_FTOF:
    {
        const size_t param_size  = necro_mach_type_calculate_size_in_bytes(context->program, ast->uop.param->necro_machine_type);
        const size_t result_size = necro_mach_type_calculate_size_in_bytes(context->program, ast->uop.result->necro_machine_type);
        if (param_size < result_size)
            value = LLVMBuildFPExt(context->builder, param, result_type, name);
        else if (param_size > result_size)
            value = LLVMBuildFPTrunc(context->builder, param, result_type, name);
        else
            value = param;
        break;
    }
    // case NECRO_PRIMOP_UOP_BREV: value = LLVMBi; break; // TODO
    case NECRO_PRIMOP_UOP_FFLR:
    {
        //--------------------
        // Floor magic
        // based somewhat on: http://blog.frama-c.com/index.php?post/2013/05/04/nearbyintf3
        // This involves a lot of magic and should only be used when speed is absolutely paramount
        // Add then Subtract: 2^52
        // IEEE754 double arithmetic will cause this to round
        // This reduces the precision of the double to integer values
        // Need some adjustments to value to create a floor and not a round
        // Works for values in the range 0-... (TODO:
        const double round_magic = pow(2, 52);
        param                    = LLVMBuildFAdd(context->builder, param, LLVMConstReal(LLVMDoubleTypeInContext(context->context), 0.5), "fflr_add");
        value                    = LLVMBuildFAdd(context->builder, param, LLVMConstReal(LLVMDoubleTypeInContext(context->context), round_magic), "round_add");
        value                    = LLVMBuildFAdd(context->builder, value, LLVMConstReal(LLVMDoubleTypeInContext(context->context), -round_magic), "round_sub");
        value                    = LLVMBuildFSub(context->builder, value, LLVMConstReal(LLVMDoubleTypeInContext(context->context), 1.0), "fflr_result");
        break;
    }

    default:
        assert(false);
        break;
    }
    NecroLLVMSymbol* symbol = necro_llvm_symbol_get(&context->arena, ast->uop.result->value.reg_symbol);
    symbol->type            = result_type;
    symbol->value           = value;
    necro_llvm_codegen_delayed_phi_node(context, symbol);
    return value;
}

LLVMValueRef necro_llvm_codegen_cmp(NecroLLVM* context, NecroMachAst* ast)
{
    assert(context != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACH_CMP);
    const char*  name  = ast->cmp.result->value.reg_symbol->name->str;
    LLVMValueRef left  = necro_llvm_codegen_value(context, ast->cmp.left);
    LLVMValueRef right = necro_llvm_codegen_value(context, ast->cmp.right);
    LLVMValueRef value = NULL;
    if (ast->cmp.left->necro_machine_type->type == NECRO_MACH_TYPE_UINT1  ||
        ast->cmp.left->necro_machine_type->type == NECRO_MACH_TYPE_UINT8  ||
        ast->cmp.left->necro_machine_type->type == NECRO_MACH_TYPE_UINT16 ||
        ast->cmp.left->necro_machine_type->type == NECRO_MACH_TYPE_UINT32 ||
        ast->cmp.left->necro_machine_type->type == NECRO_MACH_TYPE_UINT64 ||
        ast->cmp.left->necro_machine_type->type == NECRO_MACH_TYPE_PTR)
    {
        switch (ast->cmp.cmp_type)
        {
        case NECRO_PRIMOP_CMP_EQ: value = LLVMBuildICmp(context->builder, LLVMIntEQ,  left, right, name); break;
        case NECRO_PRIMOP_CMP_NE: value = LLVMBuildICmp(context->builder, LLVMIntNE,  left, right, name); break;
        case NECRO_PRIMOP_CMP_GT: value = LLVMBuildICmp(context->builder, LLVMIntUGT, left, right, name); break;
        case NECRO_PRIMOP_CMP_GE: value = LLVMBuildICmp(context->builder, LLVMIntUGE, left, right, name); break;
        case NECRO_PRIMOP_CMP_LT: value = LLVMBuildICmp(context->builder, LLVMIntULT, left, right, name); break;
        case NECRO_PRIMOP_CMP_LE: value = LLVMBuildICmp(context->builder, LLVMIntULE, left, right, name); break;
        default: assert(false); break;
        }
    }
    else if (ast->cmp.left->necro_machine_type->type == NECRO_MACH_TYPE_INT32 ||
             ast->cmp.left->necro_machine_type->type == NECRO_MACH_TYPE_INT64)
    {
        switch (ast->cmp.cmp_type)
        {
        case NECRO_PRIMOP_CMP_EQ: value = LLVMBuildICmp(context->builder, LLVMIntEQ,  left, right, name); break;
        case NECRO_PRIMOP_CMP_NE: value = LLVMBuildICmp(context->builder, LLVMIntNE,  left, right, name); break;
        case NECRO_PRIMOP_CMP_GT: value = LLVMBuildICmp(context->builder, LLVMIntSGT, left, right, name); break;
        case NECRO_PRIMOP_CMP_GE: value = LLVMBuildICmp(context->builder, LLVMIntSGE, left, right, name); break;
        case NECRO_PRIMOP_CMP_LT: value = LLVMBuildICmp(context->builder, LLVMIntSLT, left, right, name); break;
        case NECRO_PRIMOP_CMP_LE: value = LLVMBuildICmp(context->builder, LLVMIntSLE, left, right, name); break;
        default: assert(false); break;
        }
    }
    else if (ast->cmp.left->necro_machine_type->type == NECRO_MACH_TYPE_F32 ||
             ast->cmp.left->necro_machine_type->type == NECRO_MACH_TYPE_F64)
    {
        switch (ast->cmp.cmp_type)
        {
        case NECRO_PRIMOP_CMP_EQ: value = LLVMBuildFCmp(context->builder, LLVMRealUEQ,  left, right, name); break;
        case NECRO_PRIMOP_CMP_NE: value = LLVMBuildFCmp(context->builder, LLVMRealUNE,  left, right, name); break;
        case NECRO_PRIMOP_CMP_GT: value = LLVMBuildFCmp(context->builder, LLVMRealUGT, left, right, name); break;
        case NECRO_PRIMOP_CMP_GE: value = LLVMBuildFCmp(context->builder, LLVMRealUGE, left, right, name); break;
        case NECRO_PRIMOP_CMP_LT: value = LLVMBuildFCmp(context->builder, LLVMRealULT, left, right, name); break;
        case NECRO_PRIMOP_CMP_LE: value = LLVMBuildFCmp(context->builder, LLVMRealULE, left, right, name); break;
        default: assert(false); break;
        }
    }
    else
    {
        assert(false);
    }
    NecroLLVMSymbol* symbol = necro_llvm_symbol_get(&context->arena, ast->cmp.result->value.reg_symbol);
    symbol->type            = necro_llvm_type_from_mach_type(context, ast->cmp.result->necro_machine_type);
    symbol->value           = value;
    necro_llvm_codegen_delayed_phi_node(context, symbol);
    return value;
}

LLVMValueRef necro_llvm_codegen_phi(NecroLLVM* context, NecroMachAst* ast)
{
    assert(context != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACH_PHI);
    LLVMTypeRef       phi_type  = necro_llvm_type_from_mach_type(context, ast->phi.result->necro_machine_type);
    LLVMValueRef      phi_value = LLVMBuildPhi(context->builder, phi_type, ast->phi.result->value.reg_symbol->name->str);
    NecroMachPhiList* values    = ast->phi.values;
    while (values != NULL)
    {
        LLVMValueRef      leaf_value = necro_llvm_codegen_value(context, values->data.value);
        LLVMBasicBlockRef block      = necro_llvm_symbol_get(&context->arena, values->data.block->block.symbol)->block;
        if (leaf_value != NULL)
            LLVMAddIncoming(phi_value, &leaf_value, &block, 1);
        else
            necro_llvm_add_delayed_phi_node(context, values->data.value, block, phi_value);
        values = values->next;
    }
    NecroLLVMSymbol* symbol = necro_llvm_symbol_get(&context->arena, ast->phi.result->value.reg_symbol);
    symbol->type            = phi_type;
    symbol->value           = phi_value;
    necro_llvm_codegen_delayed_phi_node(context, symbol);
    return phi_value;
}

LLVMValueRef necro_llvm_codegen_bitcast(NecroLLVM* context, NecroMachAst* ast)
{
    assert(context != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACH_BIT_CAST);

    LLVMValueRef value      = necro_llvm_codegen_value(context, ast->bit_cast.from_value);
    LLVMTypeRef  value_type = LLVMTypeOf(value);
    LLVMTypeRef  to_type    = necro_llvm_type_from_mach_type(context, ast->bit_cast.to_value->necro_machine_type);
    LLVMValueRef to_value   = NULL;

    // TODO: Test if it's a pointer or an unboxed type!!!
    // Same Type
    if (value_type == to_type)
    {
        to_value = value;
    }
    else if (ast->bit_cast.from_value->necro_machine_type == context->program->type_cache.word_uint_type && ast->bit_cast.to_value->necro_machine_type->type == NECRO_MACH_TYPE_PTR)
    {
        to_value = LLVMBuildIntToPtr(context->builder, value, to_type, "uint_to_ptr");
    }
    // // Float -> Ptr
    // else if (value_type == codegen->word_float_type)
    // {
    //     LLVMValueRef float_to_int = LLVMBuildBitCast(codegen->builder, value, codegen->word_uint_type, "float_to_int");
    //     to_value = LLVMBuildIntToPtr(codegen->builder, float_to_int, to_type, "int_to_ptr");
    // }
    // // Ptr -> Float
    // else if (to_type == codegen->word_float_type)
    // {
    //     LLVMValueRef ptr_to_int = LLVMBuildPtrToInt(codegen->builder, value, codegen->word_uint_type, "ptr_to_int");
    //     to_value = LLVMBuildBitCast(codegen->builder, ptr_to_int, codegen->word_float_type, "int_to_float");
    // }
    // // Int -> Ptr
    // else if (necro_is_boxed_llvm_type(codegen, value_type))
    // {
    //     to_value = LLVMBuildIntToPtr(codegen->builder, value, to_type, "int_to_ptr");
    // }
    // Ptr -> Int
    else if (ast->bit_cast.from_value->necro_machine_type->type == NECRO_MACH_TYPE_PTR && ast->bit_cast.to_value->necro_machine_type == context->program->type_cache.word_uint_type)
    {
        to_value = LLVMBuildPtrToInt(context->builder, value, to_type, "ptr_to_uint");
    }
    // Everything else
    else
    {
        to_value = LLVMBuildBitCast(context->builder, value, to_type, ast->bit_cast.to_value->value.reg_symbol->name->str);
    }

    //--------------------
    // Store data, return
    NecroLLVMSymbol* symbol = necro_llvm_symbol_get(&context->arena, ast->bit_cast.to_value->value.reg_symbol);
    symbol->type            = to_type;
    symbol->value           = to_value;
    necro_llvm_codegen_delayed_phi_node(context, symbol);
    return to_value;
}

LLVMValueRef necro_llvm_codegen_terminator(NecroLLVM* context, NecroMachTerminator* term)
{
    assert(context != NULL);
    assert(term != NULL);
    switch (term->type)
    {
    case NECRO_MACH_TERM_RETURN:
        return LLVMBuildRet(context->builder, necro_llvm_codegen_value(context, term->return_terminator.return_value));
    case NECRO_MACH_TERM_RETURN_VOID:
        return LLVMBuildRetVoid(context->builder);
    case NECRO_MACH_TERM_BREAK:
        return LLVMBuildBr(context->builder, necro_llvm_symbol_get(&context->arena, term->break_terminator.block_to_jump_to->block.symbol)->block);
    case NECRO_MACH_TERM_COND_BREAK:
        return LLVMBuildCondBr(context->builder, necro_llvm_codegen_value(context, term->cond_break_terminator.cond_value), necro_llvm_symbol_get(&context->arena, term->cond_break_terminator.true_block->block.symbol)->block, necro_llvm_symbol_get(&context->arena, term->cond_break_terminator.false_block->block.symbol)->block);
    case NECRO_MACH_TERM_UNREACHABLE:
        return LLVMBuildUnreachable(context->builder);
    case NECRO_MACH_TERM_SWITCH:
    {
        LLVMValueRef         cond_value  = necro_llvm_codegen_value(context, term->switch_terminator.choice_val);
        LLVMBasicBlockRef    else_block  = necro_llvm_symbol_get(&context->arena, term->switch_terminator.else_block->block.symbol)->block;
        size_t               num_choices = 0;
        NecroMachSwitchList* choices     = term->switch_terminator.values;
        while (choices != NULL)
        {
            num_choices++;
            choices = choices->next;
        }
        assert(num_choices <= UINT32_MAX);
        LLVMValueRef switch_value = LLVMBuildSwitch(context->builder, cond_value, else_block, (uint32_t) num_choices);
        choices = term->switch_terminator.values;
        while (choices != NULL)
        {
            LLVMValueRef      choice_val =
                (LLVMTypeOf(cond_value) == LLVMInt32TypeInContext(context->context)) ?
                LLVMConstInt(LLVMInt32TypeInContext(context->context), choices->data.value, false) :
                LLVMConstInt(LLVMInt64TypeInContext(context->context), choices->data.value, false);
            LLVMBasicBlockRef block      = necro_llvm_symbol_get(&context->arena, choices->data.block->block.symbol)->block;
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

LLVMValueRef necro_llvm_codegen_block_statement(NecroLLVM* codegen, NecroMachAst* ast)
{
    assert(codegen != NULL);
    assert(ast != NULL);
    switch (ast->type)
    {
    case NECRO_MACH_VALUE:         return necro_llvm_codegen_value(codegen, ast);
    case NECRO_MACH_ZEXT:          return necro_llvm_codegen_zext(codegen, ast);
    case NECRO_MACH_GEP:           return necro_llvm_codegen_gep(codegen, ast);
    case NECRO_MACH_INSERT_VALUE:  return necro_llvm_codegen_insert_value(codegen, ast);
    case NECRO_MACH_EXTRACT_VALUE: return necro_llvm_codegen_extract_value(codegen, ast);
    case NECRO_MACH_CMP:           return necro_llvm_codegen_cmp(codegen, ast);
    case NECRO_MACH_PHI:           return necro_llvm_codegen_phi(codegen, ast);
    case NECRO_MACH_STORE:         return necro_llvm_codegen_store(codegen, ast);
    case NECRO_MACH_LOAD:          return necro_llvm_codegen_load(codegen, ast);
    case NECRO_MACH_BIT_CAST:      return necro_llvm_codegen_bitcast(codegen, ast);
    case NECRO_MACH_BINOP:         return necro_llvm_codegen_binop(codegen, ast);
    case NECRO_MACH_UOP:           return necro_llvm_codegen_uop(codegen, ast);
    case NECRO_MACH_CALL:          return necro_llvm_codegen_call(codegen, ast);
    case NECRO_MACH_CALLI:         return necro_llvm_codegen_call_intrinsic(codegen, ast);

    // Not currently supported
    // case NECRO_MACH_NALLOC:   return necro_llvm_codegen_nalloc(codegen, ast);
    // case NECRO_MACH_MEMCPY:   return necro_llvm_codegen_memcpy(codegen, ast);
    // case NECRO_MACH_MEMSET:   return necro_llvm_codegen_memset(codegen, ast);
    // case NECRO_MACH_ALLOCA:   return necro_codegen_alloca(codegen, ast);
    // case NECRO_MACH_SELECT:   return necro_codegen_select(codegen, ast);

    default:                     assert(false); return NULL;
    }
}

void necro_llvm_declare_function(NecroLLVM* context, NecroMachAst* ast)
{
    assert(ast->type == NECRO_MACH_FN_DEF);
    assert(ast->fn_def.fn_value->value.global_symbol->codegen_symbol == NULL);
    // Fn begin
    LLVMTypeRef      fn_type       = necro_llvm_type_from_mach_type(context, ast->necro_machine_type);
    LLVMValueRef     fn_value      = LLVMAddFunction(context->mod, ast->fn_def.symbol->name->str, fn_type);
    NecroLLVMSymbol* fn_def_symbol = necro_llvm_symbol_get(&context->arena, ast->fn_def.symbol);
    fn_def_symbol->type            = fn_type;
    fn_def_symbol->value           = fn_value;
    ast->fn_def.fn_value->value.global_symbol->codegen_symbol = fn_def_symbol;
    if (ast->fn_def.fn_type == NECRO_MACH_FN_RUNTIME)
    {
        assert(fn_def_symbol->value != NULL);
        assert(!LLVMIsNull(fn_def_symbol->value));
        assert(LLVMIsAFunction(fn_def_symbol->value));
        // printf("runtime function: %s :: %s\n", ast->fn_def.symbol->name->str, LLVMPrintTypeToString(fn_type));
        LLVMSetFunctionCallConv(fn_value, LLVMCCallConv);
        LLVMSetLinkage(fn_value, LLVMExternalLinkage);
    }
}

void necro_llvm_codegen_function(NecroLLVM* context, NecroMachAst* ast)
{
    assert(context != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACH_FN_DEF);

    // Fn begin
    NecroLLVMSymbol* fn_symbol = necro_llvm_symbol_get(&context->arena, ast->fn_def.symbol);
    if (ast->fn_def.fn_type == NECRO_MACH_FN_RUNTIME)
        return;

    LLVMValueRef     fn_value  = fn_symbol->value;
    LLVMSetFunctionCallConv(fn_value, LLVMFastCallConv);
    LLVMBasicBlockRef entry = NULL;

    // Add all blocks
    NecroMachAst* blocks = ast->fn_def.call_body;
    while (blocks != NULL)
    {
        LLVMBasicBlockRef block = LLVMAppendBasicBlock(fn_value, blocks->block.symbol->name->str);
        necro_llvm_symbol_get(&context->arena, blocks->block.symbol)->block = block;
        if (entry == NULL)
            entry = block;
        blocks = blocks->block.next_block;
    }

    // codegen bodies
    blocks = ast->fn_def.call_body;
    while (blocks != NULL)
    {
        LLVMPositionBuilderAtEnd(context->builder, necro_llvm_symbol_get(&context->arena, blocks->block.symbol)->block);
        for (size_t i = 0; i < blocks->block.num_statements; ++i)
        {
            necro_llvm_codegen_block_statement(context, blocks->block.statements[i]);
        }
        necro_llvm_codegen_terminator(context, blocks->block.terminator);
        blocks = blocks->block.next_block;
    }
    // if (context->should_optimize)
    //     LLVMRunFunctionPassManager(context->fn_pass_manager, fn_value);
}

LLVMValueRef necro_llvm_codegen_call(NecroLLVM* context, NecroMachAst* ast)
{
    assert(context != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACH_CALL);
    bool        is_void     = ast->call.result_reg->value.value_type == NECRO_MACH_VALUE_VOID;
    const char* result_name = NULL;
    if (!is_void)
        result_name = ast->call.result_reg->value.reg_symbol->name->str;
    else
        result_name = "";
    LLVMValueRef  fn_value    = necro_llvm_codegen_value(context, ast->call.fn_value);
    size_t        num_params  = ast->call.num_parameters;
    LLVMValueRef* params      = necro_paged_arena_alloc(&context->arena, num_params * sizeof(LLVMValueRef));
    for (size_t i = 0; i < num_params; ++i)
    {
        params[i] = necro_llvm_codegen_value(context, ast->call.parameters[i]);
    }
    assert(num_params <= UINT32_MAX);
    LLVMValueRef result = LLVMBuildCall(context->builder, fn_value, params, (uint32_t) num_params, result_name);
    if (ast->call.call_type == NECRO_MACH_CALL_C)
        LLVMSetInstructionCallConv(result, LLVMCCallConv);
    else
        LLVMSetInstructionCallConv(result, LLVMFastCallConv);
    if (!is_void)
    {
        NecroLLVMSymbol* symbol = necro_llvm_symbol_get(&context->arena, ast->call.result_reg->value.reg_symbol);
        symbol->type            = LLVMTypeOf(result);
        symbol->value           = result;
        necro_llvm_codegen_delayed_phi_node(context, symbol);
    }
    return result;
}

void necro_llvm_set_intrinsic_uop_type_and_value(NecroLLVM* context, NecroMachType* arg_mach_type, NecroAstSymbol* symbol_32, NecroAstSymbol* symbol_64, const char* name_32, const char* name_64, LLVMTypeRef cmp_type_32, LLVMTypeRef* fn_type, LLVMValueRef* fn_value)
{
    LLVMTypeRef arg_type = necro_llvm_type_from_mach_type(context, arg_mach_type);
    *fn_type = LLVMFunctionType(arg_type, (LLVMTypeRef[]) { arg_type }, 1, false);
    if (arg_type == cmp_type_32)
        *fn_value = necro_llvm_intrinsic_get(context, symbol_32->core_ast_symbol->mach_symbol, name_32, *fn_type);
    else
        *fn_value = necro_llvm_intrinsic_get(context, symbol_64->core_ast_symbol->mach_symbol, name_64, *fn_type);
}

void necro_llvm_set_intrinsic_binop_type_and_value(NecroLLVM* context, NecroMachType* arg_mach_type, NecroAstSymbol* symbol_32, NecroAstSymbol* symbol_64, const char* name_32, const char* name_64, LLVMTypeRef cmp_type_32, LLVMTypeRef* fn_type, LLVMValueRef* fn_value)
{
    LLVMTypeRef arg_type = necro_llvm_type_from_mach_type(context, arg_mach_type);
    *fn_type = LLVMFunctionType(arg_type, (LLVMTypeRef[]) { arg_type, arg_type }, 2, false);
    if (arg_type == cmp_type_32)
        *fn_value = necro_llvm_intrinsic_get(context, symbol_32->core_ast_symbol->mach_symbol, name_32, *fn_type);
    else
        *fn_value = necro_llvm_intrinsic_get(context, symbol_64->core_ast_symbol->mach_symbol, name_64, *fn_type);
}


LLVMValueRef necro_llvm_codegen_call_intrinsic(NecroLLVM* context, NecroMachAst* ast)
{
    assert(context != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACH_CALLI);
    bool        is_void     = ast->call_intrinsic.result_reg->value.value_type == NECRO_MACH_VALUE_VOID;
    const char* result_name = NULL;
    if (!is_void)
        result_name = ast->call_intrinsic.result_reg->value.reg_symbol->name->str;
    else
        result_name = "";
    LLVMValueRef fn_value   = NULL;
    LLVMTypeRef  fn_type    = NULL;
    LLVMTypeRef  int32_type = LLVMInt32TypeInContext(context->context);
    LLVMTypeRef  f32_type   = LLVMFloatTypeInContext(context->context);
    switch (ast->call_intrinsic.intrinsic)
    {
    case NECRO_PRIMOP_INTR_FMA:
        fn_type  = LLVMFunctionType(LLVMDoubleTypeInContext(context->context), (LLVMTypeRef[]) { LLVMDoubleTypeInContext(context->context), LLVMDoubleTypeInContext(context->context), LLVMDoubleTypeInContext(context->context) }, 3, false);
        fn_value = necro_llvm_intrinsic_get(context, context->base->fma->core_ast_symbol->mach_symbol, "llvm.fmuladd.f64", fn_type);
        break;
    case NECRO_PRIMOP_INTR_BREV:
        necro_llvm_set_intrinsic_uop_type_and_value(context, ast->necro_machine_type, context->base->bit_reverse_uint, context->base->bit_reverse_uint, "llvm.bitreverse.i32", "llvm.bitreverse.i64", int32_type, &fn_type, &fn_value);
        break;
    case NECRO_PRIMOP_INTR_FABS:
        necro_llvm_set_intrinsic_uop_type_and_value(context, ast->necro_machine_type, context->base->abs_float, context->base->abs_f64, "llvm.fabs.f32", "llvm.fabs.f64", f32_type, &fn_type, &fn_value);
        break;
    case NECRO_PRIMOP_INTR_SIN:
        necro_llvm_set_intrinsic_uop_type_and_value(context, ast->necro_machine_type, context->base->sine_float, context->base->sine_f64, "llvm.sin.f32", "llvm.sin.f64", f32_type, &fn_type, &fn_value);
        break;
    case NECRO_PRIMOP_INTR_COS:
        necro_llvm_set_intrinsic_uop_type_and_value(context, ast->necro_machine_type, context->base->cosine_float, context->base->cosine_f64, "llvm.cos.f32", "llvm.cos.f64", f32_type, &fn_type, &fn_value);
        break;
    case NECRO_PRIMOP_INTR_EXP:
        necro_llvm_set_intrinsic_uop_type_and_value(context, ast->necro_machine_type, context->base->exp_float, context->base->exp_f64, "llvm.exp.f32", "llvm.exp.f64", f32_type, &fn_type, &fn_value);
        break;
    case NECRO_PRIMOP_INTR_EXP2:
        necro_llvm_set_intrinsic_uop_type_and_value(context, ast->necro_machine_type, context->base->exp2_float, context->base->exp2_f64, "llvm.exp2.f32", "llvm.exp2.f64", f32_type, &fn_type, &fn_value);
        break;
    case NECRO_PRIMOP_INTR_LOG:
        necro_llvm_set_intrinsic_uop_type_and_value(context, ast->necro_machine_type, context->base->log_float, context->base->log_f64, "llvm.log.f32", "llvm.log.f64", f32_type, &fn_type, &fn_value);
        break;
    case NECRO_PRIMOP_INTR_LOG10:
        necro_llvm_set_intrinsic_uop_type_and_value(context, ast->necro_machine_type, context->base->log10_float, context->base->log10_f64, "llvm.log10.f32", "llvm.log10.f64", f32_type, &fn_type, &fn_value);
        break;
    case NECRO_PRIMOP_INTR_LOG2:
        necro_llvm_set_intrinsic_uop_type_and_value(context, ast->necro_machine_type, context->base->log2_float, context->base->log2_f64, "llvm.log2.f32", "llvm.log2.f64", f32_type, &fn_type, &fn_value);
        break;
    case NECRO_PRIMOP_INTR_POW:
        necro_llvm_set_intrinsic_binop_type_and_value(context, ast->necro_machine_type, context->base->pow_float, context->base->pow_f64, "llvm.pow.f32", "llvm.pow.f64", f32_type, &fn_type, &fn_value);
        break;
    case NECRO_PRIMOP_INTR_SQRT:
        necro_llvm_set_intrinsic_uop_type_and_value(context, ast->necro_machine_type, context->base->sqrt_float, context->base->sqrt_f64, "llvm.sqrt.f32", "llvm.sqrt.f64", f32_type, &fn_type, &fn_value);
        break;
    // case NECRO_PRIMOP_INTR_FLR:
    // {
    //     fn_type  = LLVMFunctionType(LLVMDoubleTypeInContext(context->context), (LLVMTypeRef[]) { LLVMDoubleTypeInContext(context->context) }, 1, false);
    //     fn_value = necro_llvm_intrinsic_get(context, context->base->floor->core_ast_symbol->mach_symbol, "llvm.floor.f64", fn_type);
    //     break;
    // }
    default:
        assert(false);
        break;
    }
    size_t        num_params  = ast->call_intrinsic.num_parameters;
    LLVMValueRef* params      = necro_paged_arena_alloc(&context->arena, num_params * sizeof(LLVMValueRef));
    for (size_t i = 0; i < num_params; ++i)
    {
        params[i] = necro_llvm_codegen_value(context, ast->call_intrinsic.parameters[i]);
    }
    LLVMValueRef result = LLVMBuildCall(context->builder, fn_value, params, (unsigned int) num_params, result_name);
    LLVMSetInstructionCallConv(result, LLVMGetFunctionCallConv(fn_value));
    // LLVMSetInstructionCallConv(result, LLVMFastCallConv);
    if (!is_void)
    {
        NecroLLVMSymbol* symbol = necro_llvm_symbol_get(&context->arena, ast->call_intrinsic.result_reg->value.reg_symbol);
        symbol->type            = LLVMTypeOf(result);
        symbol->value           = result;
        necro_llvm_codegen_delayed_phi_node(context, symbol);
    }
    return result;
}

void necro_codegen_global(NecroLLVM* context, NecroMachAst* ast)
{
    assert(context != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_MACH_VALUE);
    assert(ast->value.value_type == NECRO_MACH_VALUE_GLOBAL);
    NecroLLVMSymbol* global_symbol = necro_llvm_symbol_get(&context->arena, ast->value.global_symbol);
    LLVMTypeRef      global_type   = necro_llvm_type_from_mach_type(context, ast->necro_machine_type->ptr_type.element_type);
    const char*      global_name   = ast->value.global_symbol->name->str;
    LLVMValueRef     global_value  = LLVMAddGlobal(context->mod, global_type, global_name);
    global_symbol->type            = global_type;
    global_symbol->value           = global_value;
    LLVMSetLinkage(global_value, LLVMInternalLinkage);
    if (global_symbol->mach_symbol->global_string_symbol == NULL)
    {
        LLVMValueRef zero_value = LLVMConstNull(global_type);
        LLVMSetInitializer(global_value, zero_value);
        LLVMSetGlobalConstant(global_value, false);
    }
    else
    {
        // TODO / NOTE: Strings are represented incorrectly in literals. They should be const size_t* to match their representation in necro lang, NOT const char*!!!!
        const size_t  str_length   = global_symbol->mach_symbol->global_string_symbol->length;
        LLVMValueRef  string_value = NULL;
        LLVMTypeRef   element_type = (context->program->word_size == NECRO_WORD_4_BYTES) ? LLVMInt32TypeInContext(context->context) : LLVMInt64TypeInContext(context->context);
        LLVMValueRef* chars        = necro_paged_arena_alloc(&context->program->arena, str_length * sizeof(LLVMValueRef));
        const char*   str          = global_symbol->mach_symbol->global_string_symbol->str;
        for (size_t i = 0; i < str_length; ++i)
        {
            assert(str != NULL);
            assert(*str != '\0');
            chars[i] = LLVMConstInt(element_type, *str, false);
            str++;
        }
        string_value = LLVMConstArray(element_type, chars, (unsigned int) str_length);
        LLVMSetInitializer(global_value, string_value);
        LLVMSetGlobalConstant(global_value, true);
    }
}

void necro_llvm_map_check_symbol(NecroMachAstSymbol* mach_symbol)
{
    if (mach_symbol == NULL)
        return;
    NecroLLVMSymbol* llvm_symbol = mach_symbol->ast->fn_def.fn_value->value.global_symbol->codegen_symbol;
    assert(llvm_symbol->value != NULL);
    if (LLVMGetFirstUse(llvm_symbol->value) == NULL)
        llvm_symbol->value = NULL;
}

void necro_llvm_map_runtime_symbol(LLVMExecutionEngineRef engine, NecroMachAstSymbol* mach_symbol)
{
    if (mach_symbol == NULL)
        return;
    NecroLLVMSymbol* llvm_symbol = mach_symbol->ast->fn_def.fn_value->value.global_symbol->codegen_symbol;
    if (llvm_symbol->value == NULL)
        return;
    assert(llvm_symbol->value != NULL);
    assert(!LLVMIsNull(llvm_symbol->value));
    assert(LLVMIsAFunction(llvm_symbol->value));
    LLVMAddGlobalMapping(engine, llvm_symbol->value, (void*) mach_symbol->ast->fn_def.runtime_fn_addr);
}


///////////////////////////////////////////////////////
// Necro Codegen Go
///////////////////////////////////////////////////////
void necro_llvm_codegen(NecroCompileInfo info, NecroMachProgram* program, NecroLLVM* context)
{
    *context = necro_llvm_create(program->intern, program->base, program, info.opt_level > 0);
    // *context = necro_llvm_create(program->intern, program->base, program, true);

    // Declare structs
    for (size_t i = 0; i < program->structs.length; ++i)
    {
        necro_llvm_type_from_mach_type(context, program->structs.data[i]->necro_machine_type);
    }
    // Declare machine structs
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        necro_llvm_type_from_mach_type(context, program->machine_defs.data[i]->necro_machine_type);
    }
    // codegen globals
    for (size_t i = 0; i < program->globals.length; ++i)
    {
        necro_codegen_global(context, program->globals.data[i]);
    }
    // Declare functions
    for (size_t i = 0; i < program->functions.length; ++i)
    {
        necro_llvm_declare_function(context, program->functions.data[i]);
    }
    // Declare machine init_fns
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        if (program->machine_defs.data[i]->machine_def.init_fn != NULL)
            necro_llvm_declare_function(context, program->machine_defs.data[i]->machine_def.init_fn);
    }
    // Declare machine mk_fns
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        if (program->machine_defs.data[i]->machine_def.mk_fn != NULL)
            necro_llvm_declare_function(context, program->machine_defs.data[i]->machine_def.mk_fn);
    }
    // Declare machine update_fns
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        necro_llvm_declare_function(context, program->machine_defs.data[i]->machine_def.update_fn);
    }
    // Declare main_fn
    necro_llvm_declare_function(context, program->necro_init);
    necro_llvm_declare_function(context, program->necro_main);
    necro_llvm_declare_function(context, program->necro_shutdown);
    // codegen functions
    for (size_t i = 0; i < program->functions.length; ++i)
    {
        necro_llvm_codegen_function(context, program->functions.data[i]);
    }
    // codegen machine init_fns
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        if (program->machine_defs.data[i]->machine_def.init_fn != NULL)
            necro_llvm_codegen_function(context, program->machine_defs.data[i]->machine_def.init_fn);
    }
    // codegen machine mk_fns
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        if (program->machine_defs.data[i]->machine_def.mk_fn != NULL)
            necro_llvm_codegen_function(context, program->machine_defs.data[i]->machine_def.mk_fn);
    }
    // codegen machine update_fns
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        necro_llvm_codegen_function(context, program->machine_defs.data[i]->machine_def.update_fn);
    }
    // codegen main
    necro_llvm_codegen_function(context, program->necro_init);
    necro_llvm_codegen_function(context, program->necro_main);
    necro_llvm_codegen_function(context, program->necro_shutdown);

    //--------------------
    // Check runtime function usage
    necro_llvm_map_check_symbol(context->program->runtime.necro_init_runtime);
    necro_llvm_map_check_symbol(context->program->runtime.necro_update_runtime);
    necro_llvm_map_check_symbol(context->program->runtime.necro_error_exit);
    necro_llvm_map_check_symbol(context->program->runtime.necro_inexhaustive_case_exit);
    necro_llvm_map_check_symbol(context->program->runtime.necro_print);
    necro_llvm_map_check_symbol(context->program->runtime.necro_print_char);
    necro_llvm_map_check_symbol(context->program->runtime.necro_runtime_print_string);
    necro_llvm_map_check_symbol(context->base->print_int->core_ast_symbol->mach_symbol);
    necro_llvm_map_check_symbol(context->base->print_i64->core_ast_symbol->mach_symbol);
    necro_llvm_map_check_symbol(context->base->print_uint->core_ast_symbol->mach_symbol);
    necro_llvm_map_check_symbol(context->base->print_float->core_ast_symbol->mach_symbol);
    necro_llvm_map_check_symbol(context->base->print_f64->core_ast_symbol->mach_symbol);
    necro_llvm_map_check_symbol(context->program->runtime.necro_runtime_get_mouse_x);
    necro_llvm_map_check_symbol(context->program->runtime.necro_runtime_get_mouse_y);
    necro_llvm_map_check_symbol(context->program->runtime.necro_runtime_is_done);
    necro_llvm_map_check_symbol(context->program->runtime.necro_runtime_alloc);
    necro_llvm_map_check_symbol(context->program->runtime.necro_runtime_realloc);
    necro_llvm_map_check_symbol(context->program->runtime.necro_runtime_free);
    necro_llvm_map_check_symbol(context->base->panic->core_ast_symbol->mach_symbol);
    necro_llvm_map_check_symbol(context->program->runtime.necro_runtime_out_audio_block);
    necro_llvm_map_check_symbol(context->base->floor->core_ast_symbol->mach_symbol);
    necro_llvm_map_check_symbol(context->base->sinh_f64->core_ast_symbol->mach_symbol);
    necro_llvm_map_check_symbol(context->base->test_assertion->core_ast_symbol->mach_symbol);

    // assert(context->delayed_phi_node_values.length == 0);
    if (context->should_optimize)
        LLVMRunPassManager(context->mod_pass_manager, context->mod);
    // verify and print
    necro_llvm_verify_and_dump(context);
    if ((info.compilation_phase == NECRO_PHASE_CODEGEN && info.verbosity > 0) || info.verbosity > 1)
    {
        necro_llvm_print(context);
    }

}

///////////////////////////////////////////////////////
// Necro JIT
///////////////////////////////////////////////////////
void necro_fatal_error_handler(const char* error)
{
    printf("necro fatal error: %s", error);
    exit(1);
}

NecroLangCallback* necro_llvm_get_lang_call(NecroLLVM* context, NecroMachAstSymbol* mach_symbol)
{
    NecroLLVMSymbol*   llvm_symbol = necro_llvm_symbol_get(&context->arena, mach_symbol);
    assert(llvm_symbol != NULL);
    LLVMSetFunctionCallConv(llvm_symbol->value, LLVMCCallConv);
    NecroLangCallback* necro_fn    = (NecroLangCallback*)(LLVMGetPointerToGlobal(context->engine, llvm_symbol->value));
    assert(necro_fn != NULL);
    return necro_fn;
}

void necro_llvm_jit_go(NecroCompileInfo info, NecroLLVM* context, const char* jit_string)
{
    UNUSED(info);

    //--------------------
    // Set up JIT
    char* error = NULL;
    struct LLVMMCJITCompilerOptions options;
    LLVMInitializeMCJITCompilerOptions(&options, sizeof(options));
    options.OptLevel       = context->opt_level;
    options.EnableFastISel = true;
    if (LLVMCreateMCJITCompilerForModule(&context->engine, context->mod, &options, sizeof(options), &error) != 0)
    {
        fprintf(stderr, "necro error: %s\n", error);
        LLVMDisposeMessage(error);
        assert(false);
    }
    LLVMDisposeMessage(error);

    // if (LLVMCreateExecutionEngineForModule(&context->engine, context->mod, &error) != 0)
    // {
    //     fprintf(stderr, "necro error: %s\n", error);
    //     LLVMDisposeMessage(error);
    //     assert(false);
    // }
    // LLVMDisposeMessage(error);

    //--------------------
    // Map runtime functions
    necro_llvm_map_runtime_symbol(context->engine, context->program->runtime.necro_init_runtime);
    necro_llvm_map_runtime_symbol(context->engine, context->program->runtime.necro_update_runtime);
    necro_llvm_map_runtime_symbol(context->engine, context->program->runtime.necro_error_exit);
    necro_llvm_map_runtime_symbol(context->engine, context->program->runtime.necro_inexhaustive_case_exit);
    necro_llvm_map_runtime_symbol(context->engine, context->program->runtime.necro_print);
    necro_llvm_map_runtime_symbol(context->engine, context->program->runtime.necro_print_char);
    necro_llvm_map_runtime_symbol(context->engine, context->program->runtime.necro_runtime_print_string);
    necro_llvm_map_runtime_symbol(context->engine, context->base->print_int->core_ast_symbol->mach_symbol);
    necro_llvm_map_runtime_symbol(context->engine, context->base->print_i64->core_ast_symbol->mach_symbol);
    necro_llvm_map_runtime_symbol(context->engine, context->base->print_uint->core_ast_symbol->mach_symbol);
    necro_llvm_map_runtime_symbol(context->engine, context->base->print_float->core_ast_symbol->mach_symbol);
    necro_llvm_map_runtime_symbol(context->engine, context->base->print_f64->core_ast_symbol->mach_symbol);
    necro_llvm_map_runtime_symbol(context->engine, context->program->runtime.necro_runtime_get_mouse_x);
    necro_llvm_map_runtime_symbol(context->engine, context->program->runtime.necro_runtime_get_mouse_y);
    necro_llvm_map_runtime_symbol(context->engine, context->program->runtime.necro_runtime_is_done);
    necro_llvm_map_runtime_symbol(context->engine, context->program->runtime.necro_runtime_alloc);
    necro_llvm_map_runtime_symbol(context->engine, context->program->runtime.necro_runtime_realloc);
    necro_llvm_map_runtime_symbol(context->engine, context->program->runtime.necro_runtime_free);
    necro_llvm_map_runtime_symbol(context->engine, context->base->panic->core_ast_symbol->mach_symbol);
    necro_llvm_map_runtime_symbol(context->engine, context->program->runtime.necro_runtime_out_audio_block);
    necro_llvm_map_runtime_symbol(context->engine, context->base->floor->core_ast_symbol->mach_symbol);
    necro_llvm_map_runtime_symbol(context->engine, context->base->sinh_f64->core_ast_symbol->mach_symbol);
    necro_llvm_map_runtime_symbol(context->engine, context->base->test_assertion->core_ast_symbol->mach_symbol);

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
    puts("    by  Somniloquist (Curtis McKinney)");
    puts("    and SeppukuZombie (Chad McKinney)");
    puts("\n");

    LLVMInstallFatalErrorHandler(necro_fatal_error_handler);

    NecroLangCallback* necro_init     = necro_llvm_get_lang_call(context, context->program->necro_init->fn_def.symbol);
    NecroLangCallback* necro_main     = necro_llvm_get_lang_call(context, context->program->necro_main->fn_def.symbol);
    NecroLangCallback* necro_shutdown = necro_llvm_get_lang_call(context, context->program->necro_shutdown->fn_def.symbol);

    // TODO: When to call necro_runtime_audio_init? Putting it here for now..
    // unwrap(void, necro_runtime_audio_init());
    unwrap(void, necro_runtime_audio_start(necro_init, necro_main, necro_shutdown));
    // unwrap(void, necro_runtime_audio_shutdown());
    if (!necro_runtime_was_test_successful())
    {
        printf("\n!!!!!!!!!!!!!!Test Failed!!!!!!!!!!!!!!\n\n%s\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n", jit_string);
        assert(necro_runtime_was_test_successful());
    }
}

void necro_llvm_jit(NecroCompileInfo info, NecroLLVM* context)
{
    necro_llvm_jit_go(info, context, "");
}


///////////////////////////////////////////////////////
// Necro Compile
///////////////////////////////////////////////////////
void necro_llvm_compile(NecroCompileInfo info, NecroLLVM* context)
{
    UNUSED(info);
    char* emit_error = NULL;
#ifdef _WIN32
    char* output_location = "./build/necro_test.asm";
#else
	char* output_location = "../necro_test.asm";
#endif
    if (LLVMTargetMachineEmitToFile(context->target_machine, context->mod, output_location, LLVMAssemblyFile, &emit_error) != 0)
    // char* output_location = "./build/necro_test.o";
    // if (LLVMTargetMachineEmitToFile(context->target_machine, context->mod, output_location, LLVMObjectFile, &emit_error) != 0)
    {
        fprintf(stderr, "LLVMTargetMachineEmitToFile error: %s\n", emit_error);
        LLVMDisposeMessage(emit_error);
        exit(1);
        return;
    }
    printf("Program compiled and written to: %s\n", output_location);
}

///////////////////////////////////////////////////////
// Testing
///////////////////////////////////////////////////////
#define NECRO_LLVM_TEST_VERBOSE 0
void necro_llvm_test_string_go(const char* test_name, const char* str, NECRO_PHASE phase)
{

    //--------------------
    // Set up
    NecroIntern         intern          = necro_intern_create();
    NecroScopedSymTable scoped_symtable = necro_scoped_symtable_create();
    NecroBase           base            = necro_base_compile(&intern, &scoped_symtable);

    NecroLexTokenVector tokens          = necro_empty_lex_token_vector();
    NecroParseAstArena  parse_ast       = necro_parse_ast_arena_empty();
    NecroAstArena       ast             = necro_ast_arena_empty();
    NecroCoreAstArena   core_ast        = necro_core_ast_arena_empty();
    NecroMachProgram    mach_program    = necro_mach_program_empty();
    NecroLLVM           llvm            = necro_llvm_empty();
    NecroCompileInfo    info            = necro_test_compile_info();
    // if (phase == NECRO_PHASE_JIT || phase == NECRO_PHASE_COMPILE)
        info.opt_level = 0;
    info.verbosity = 0;

    //--------------------
    // Compile
    unwrap_or_print_error(void, necro_lex(info, &intern, str, strlen(str), &tokens), str, "Test");
    unwrap_or_print_error(void, necro_parse(info, &intern, &tokens, necro_intern_string(&intern, "Test"), &parse_ast), str, "Test");
    ast = necro_reify(info, &intern, &parse_ast);
    necro_build_scopes(info, &scoped_symtable, &ast);
    unwrap_or_print_error(void, necro_rename(info, &scoped_symtable, &intern, &ast), str, "Test");
    necro_dependency_analyze(info, &intern, &base, &ast);
    necro_alias_analysis(info, &ast); // NOTE: Consider merging alias_analysis into RENAME_VAR phase?
    unwrap_or_print_error(void, necro_infer(info, &intern, &scoped_symtable, &base, &ast), str, "Test");
    unwrap_or_print_error(void, necro_monomorphize(info, &intern, &scoped_symtable, &base, &ast), str, "Test");
    unwrap_or_print_error(void, necro_ast_transform_to_core(info, &intern, &base, &ast, &core_ast), str, "Test");
    unwrap_or_print_error(void, necro_core_infer(&intern, &base, &core_ast), str, "Test");
    necro_core_ast_pre_simplify(info, &intern, &base, &core_ast);
    necro_core_lambda_lift(info, &intern, &base, &core_ast);
    unwrap_or_print_error(void, necro_core_infer(&intern, &base, &core_ast), str, "Test");
    necro_core_defunctionalize(info, &intern, &base, &core_ast);
    unwrap_or_print_error(void, necro_core_infer(&intern, &base, &core_ast), str, "Test");
    necro_core_ast_pre_simplify(info, &intern, &base, &core_ast);
    necro_core_state_analysis(info, &intern, &base, &core_ast);
    necro_core_transform_to_mach(info, &intern, &base, &core_ast, &mach_program);
    necro_llvm_codegen(info, &mach_program, &llvm);
    if (phase == NECRO_PHASE_JIT)
        necro_llvm_jit_go(info, &llvm, str);
    else if (phase == NECRO_PHASE_COMPILE)
        necro_llvm_compile(info, &llvm);

    //--------------------
    // Print
#if NECRO_LLVM_TEST_VERBOSE
    if (phase == NECRO_PHASE_CODEGEN)
    necro_llvm_print(&llvm);
    // if (phase == NECRO_PHASE_CODEGEN)
    // {
        // for (size_t i = 0; i < mach_program.machine_defs.length; ++i)
        // {
        //     if (strcmp("Necro.Base.mapAudio2b8f", mach_program.machine_defs.data[i]->machine_def.symbol->name->str) == 0)
        //     {
        //         LLVMDumpValue(mach_program.machine_defs.data[i]->machine_def.update_fn->fn_def.symbol->codegen_symbol->value);
        //     }
        // }
    // }
#endif
    printf("NecroLLVM %s test: Passed\n", test_name);
    fflush(stdout);

    //--------------------
    // Clean up
    necro_llvm_destroy(&llvm);
    necro_mach_program_destroy(&mach_program);
    necro_core_ast_arena_destroy(&core_ast);
    necro_ast_arena_destroy(&ast);
    necro_base_destroy(&base);
    necro_parse_ast_arena_destroy(&parse_ast);
    necro_destroy_lex_token_vector(&tokens);
    necro_scoped_symtable_destroy(&scoped_symtable);
    necro_intern_destroy(&intern);
}

void necro_llvm_test_string(const char* test_name, const char* str)
{
    necro_llvm_test_string_go(test_name, str, NECRO_PHASE_CODEGEN);
}

void necro_llvm_jit_string(const char* test_name, const char* str)
{
    necro_llvm_test_string_go(test_name, str, NECRO_PHASE_JIT);
}

void necro_llvm_compile_string(const char* test_name, const char* str)
{
    necro_llvm_test_string_go(test_name, str, NECRO_PHASE_COMPILE);
}

void necro_llvm_test()
{
    necro_announce_phase("LLVM");

/*

*/

    {
        const char* test_name   = "Data 1";
        const char* test_source = ""
            "data TwoInts = TwoInts Int Int\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Data 2";
        const char* test_source = ""
            "data TwoInts = TwoInts Int Int\n"
            "coolInts :: TwoInts\n"
            "coolInts = TwoInts 333 666\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Data 3";
        const char* test_source = ""
            "data TwoInts   = TwoInts Int Int\n"
            "data DoubleUp  = DoubleUp TwoInts TwoInts\n"
            "data TwoOrFour = Two TwoInts | Four DoubleUp DoubleUp\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Bind 1";
        const char* test_source = ""
            "data TwoInts = TwoInts Int Int\n"
            "twoForOne :: Int -> TwoInts\n"
            "twoForOne i = TwoInts i i\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Bind 2";
        const char* test_source = ""
            "data TwoInts   = TwoInts Int Int\n"
            "data DoubleUp  = DoubleUp TwoInts TwoInts\n"
            "doubleDown :: Int -> DoubleUp\n"
            "doubleDown i = DoubleUp (TwoInts i i) (TwoInts i i)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Main 1";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = print (add 1 2) w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Main 2";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = print mouseY (print mouseX w)\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Main 3";
        const char* test_source = ""
            "data TwoInts  = TwoInts Int Int\n"
            "data DoubleUp = DoubleUp TwoInts TwoInts\n"
            "doubleDown :: Int -> DoubleUp\n"
            "doubleDown i = DoubleUp (TwoInts i i) (TwoInts i i)\n"
            "rune :: DoubleUp\n"
            "rune = doubleDown mouseX\n"
            "main :: *World -> *World\n"
            "main w = print 0 w where\n"
            "  x = doubleDown 666\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 2";
        const char* test_source = ""
            "data SomeOrNone = Some Int | None\n"
            "main w =\n"
            "  case None of\n"
            "    None -> print 0 w\n"
            "    _    -> print 1 w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 3";
        const char* test_source = ""
            "data SomeOrNone = Some Int | None\n"
            "main w =\n"
            "  case None of\n"
            "    None   -> print 0 w\n"
            "    Some _ -> print 1 w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 4";
        const char* test_source = ""
            "data SomeOrNone = Some Int | None\n"
            "main w =\n"
            "  case Some 666 of\n"
            "    None   -> print 0 w\n"
            "    Some i -> print i w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 5";
        const char* test_source = ""
            "data TwoOfAKind = TwoOfAKind Int Int\n"
            "data SomeOrNone = Some TwoOfAKind | None\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case Some (TwoOfAKind 6 7) of\n"
            "    None -> w\n"
            "    Some (TwoOfAKind i1 i2) -> print i2 (print i1 w)\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 6";
        const char* test_source = ""
            "data TwoOfAKind = TwoOfAKind Int Int\n"
            "main w =\n"
            "  case TwoOfAKind 6 7 of\n"
            "    TwoOfAKind i1 i2 -> print i2 (print i1 w)\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 7";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w =\n"
            "  case 0 of\n"
            "    i -> print i w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 8";
        const char* test_source = ""
            "data Inner = SomeInner Int | NoInner\n"
            "data Outer = SomeOuter Inner | NoOuter\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case SomeOuter (SomeInner 6) of\n"
            "    NoOuter -> w\n"
            "    SomeOuter NoInner -> w\n"
            "    SomeOuter (SomeInner i) -> print i w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 9";
        const char* test_source = ""
            "data TwoForOne = One Int | Two Int Int\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case One 666 of\n"
            "    One x -> print x w\n"
            "    Two y z -> print z (print y w)\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 10";
        const char* test_source = ""
            "data TwoForOne = One Int | Two Int Int\n"
            "data MaybeTwoForOne = JustTwoForOne TwoForOne | NoneForOne\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case JustTwoForOne (One mouseX) of\n"
            "    NoneForOne              -> w\n"
            "    JustTwoForOne (One x)   -> print x w\n"
            "    JustTwoForOne (Two y z) -> print z (print y w)\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 11";
        const char* test_source = ""
            "data OneOrZero    = One Int | Zero\n"
            "data MoreThanZero = MoreThanZero OneOrZero OneOrZero\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case MoreThanZero (One 0) Zero of\n"
            "    MoreThanZero (One i1)  (One i2) -> print i2 (print i1 w)\n"
            "    MoreThanZero Zero      (One i3) -> print i3 w\n"
            "    MoreThanZero (One i4)  Zero     -> print i4 w\n"
            "    MoreThanZero Zero      Zero     -> w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 12";
        const char* test_source = ""
            "data TwoForOne  = One Int | Two Int Int\n"
            "data FourForOne = MoreThanZero TwoForOne TwoForOne | Zero\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case Zero of\n"
            "    Zero -> w\n"
            "    MoreThanZero (One i1)     (One i2)      -> print i2 (print i1 w)\n"
            "    MoreThanZero (Two i3 i4)  (One i5)      -> print i5 (print i4 (print i3 w))\n"
            "    MoreThanZero (One i6)     (Two i7  i8)  -> print i8 (print i7 (print i6 w))\n"
            "    MoreThanZero (Two i9 i10) (Two i11 i12) -> print i12 (print i11 (print i10 (print i9 w)))\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 13";
        const char* test_source = ""
            "data OneOrZero    = One Int | Zero\n"
            "data MoreThanZero = MoreThanZero OneOrZero OneOrZero\n"
            "oneUp :: MoreThanZero -> MoreThanZero\n"
            "oneUp m =\n"
            "  case m of\n"
            "    MoreThanZero (One i1)  (One i2) -> m\n"
            "    MoreThanZero Zero      (One i3) -> MoreThanZero (One i3) (One i3)\n"
            "    MoreThanZero (One i4)  Zero     -> MoreThanZero (One i4) (One i4)\n"
            "    MoreThanZero Zero      Zero     -> m\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Poly 1";
        const char* test_source = ""
            "nothingFromSomething :: Maybe Int -> Maybe Int\n"
            "nothingFromSomething m = m\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Poly 3";
        const char* test_source = ""
            "nothingFromSomething :: (Maybe Float, Maybe Int) -> (Maybe Float, Maybe Int)\n"
            "nothingFromSomething m = m\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Poly 5";
        const char* test_source = ""
            "nothingFromSomething :: Int -> (Maybe Float, Maybe Int)\n"
            "nothingFromSomething i = (Nothing, Just i)\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case nothingFromSomething 22 of\n"
            "    (_, Just i) -> print i w\n"
            "    _           -> print -1 w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 1";
        const char* test_source = ""
            "main w =\n"
            "  case 0 of\n"
            "    0 -> print 0 w\n"
            "    1 -> print 1 w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 2";
        const char* test_source = ""
            "main w =\n"
            "  case False of\n"
            "    False -> print 0 w\n"
            "    True  -> print 1 w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 4";
        const char* test_source = ""
            "basketCase :: Int -> *World -> *World\n"
            "basketCase i w =\n"
            "  case i of\n"
            "    0 -> print 666 w\n"
            "    u -> print u w\n"
            "main :: *World -> *World\n"
            "main w = basketCase 0 w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 5";
        const char* test_source = ""
            "basketCase :: Int -> *World -> *World\n"
            "basketCase i w =\n"
            "  case i of\n"
            "    u -> print u w\n"
            "main :: *World -> *World\n"
            "main w = basketCase 0 w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 6";
        const char* test_source = ""
            "basketCase :: Maybe Int -> *World -> *World\n"
            "basketCase m w =\n"
            "  case m of\n"
            "    Nothing -> print 100 w\n"
            "    Just 0  -> print 200 w\n"
            "    Just -1 -> print 300 w\n"
            "    Just _  -> print 400 w\n"
            "main :: *World -> *World\n"
            "main w = basketCase (Just -1) w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 7";
        const char* test_source = ""
            "basketCase :: Int -> *World -> *World\n"
            "basketCase i w =\n"
            "  case gt i 0 of\n"
            "    True -> print 666 w\n"
            "    _    -> print 777 w\n"
            "main :: *World -> *World\n"
            "main w = basketCase 0 w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 9";
        const char* test_source = ""
            "basketCase :: Int -> *World -> *World\n"
            "basketCase i w =\n"
            "  case Just (gt i 0) of\n"
            "    Nothing    -> print 0 w\n"
            "    Just True  -> print 666 w\n"
            "    Just False -> print 777 w\n"
            "main :: *World -> *World\n"
            "main w = basketCase 0 w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 10";
        const char* test_source = ""
            "basketCase :: Int -> *World -> *World\n"
            "basketCase i w =\n"
            "  case Just (gt i 0) of\n"
            "    Nothing    -> print 0 w\n"
            "    Just False -> print 777 w\n"
            "    Just _     -> print 666 w\n"
            "main :: *World -> *World\n"
            "main w = basketCase 0 w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Array 1";
        const char* test_source = ""
            "arrayed :: Array 2 Float -> Array 2 Float\n"
            "arrayed x = x\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Array 2";
        const char* test_source = ""
            "arrayed :: Maybe (Array 2 Float) -> Maybe (Array 2 Float)\n"
            "arrayed x = x\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Array 2.5";
        const char* test_source = ""
            "arrayed :: Array 2 Float\n"
            "arrayed = { 0, 1 }\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Array 3";
        const char* test_source = ""
            "arrayed :: Array 2 (Maybe Float)\n"
            "arrayed = { Nothing, Just 1 }\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Array 4";
        const char* test_source = ""
            "justMaybe :: Float -> Maybe Float\n"
            "justMaybe f = Just f\n"
            "arrayed :: Array 2 (Maybe Float)\n"
            "arrayed = { justMaybe 666, Just 1 }\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Index 1";
        const char* test_source = ""
            "iiCaptain :: Index 666 -> Index 666\n"
            "iiCaptain i = i\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "For Loop 0";
        const char* test_source = ""
            "tenTimes :: Range 10\n"
            "tenTimes = each\n"
            "main :: *World -> *World\n"
            "main w = print 0 w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "For Loop 1";
        const char* test_source = ""
            "tenTimes :: Range 10\n"
            "tenTimes = each\n"
            "loopTenTimes :: Int\n"
            "loopTenTimes =\n"
            "  loop x = 0 for i <- tenTimes do\n"
            "    add x 1\n"
            "main :: *World -> *World\n"
            "main w = print loopTenTimes w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "For Loop 1.5";
        const char* test_source = ""
            "tenTimes :: Range 10\n"
            "tenTimes = each\n"
            "loopTenTimes :: Int\n"
            "loopTenTimes =\n"
            "  loop x = mouseX for i <- tenTimes do\n"
            "    mul x 2\n"
            "main :: *World -> *World\n"
            "main w = print loopTenTimes w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "For Loop 2";
        const char* test_source = ""
            "tenTimes :: Range 10\n"
            "tenTimes = each\n"
            "loopTenTimes :: Int\n"
            "loopTenTimes =\n"
            "  loop x1 = 0 for i1 <- tenTimes do\n"
            "    loop x2 = x1 for i2 <- tenTimes do\n"
            "      add x1 1\n"
            "main :: *World -> *World\n"
            "main w = print loopTenTimes w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    // {
    //     const char* test_name   = "For Loop 3";
    //     const char* test_source = ""
    //         "tenTimes :: Range 10\n"
    //         "tenTimes = each\n"
    //         "loopTenTimes :: (Int, Int)\n"
    //         "loopTenTimes = (for tenTimes 0 loop i x -> mul x 2, for tenTimes 0 loop i x -> add x 2)\n"
    //         "main :: *World -> *World\n"
    //         "main w = w\n";
    //     necro_llvm_test_string(test_name, test_source);
    // }

    {
        const char* test_name   = "For Loop 4";
        const char* test_source = ""
            "tenTimes :: Range 10\n"
            "tenTimes = each\n"
            "loopTenTimes :: Maybe Int -> Int\n"
            "loopTenTimes m =\n"
            "  case m of\n"
            "    Nothing ->\n"
            "      loop x = 0 for i <- tenTimes do\n"
            "        add x 1\n"
            "    Just y  ->\n"
            "      loop x = 0 for i <- tenTimes do\n"
            "        add x y\n"
            "main :: *World -> *World\n"
            "main w = print (loopTenTimes (Just mouseX)) w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "For Loop 5";
        const char* test_source = ""
            "tenTimes :: Range 10\n"
            "tenTimes = each\n"
            "loopTenTimes :: Maybe UInt -> Maybe UInt\n"
            "loopTenTimes m =\n"
            "  loop x = m for i <- tenTimes do\n"
            "    case x of\n"
            "      Nothing -> Just 0\n"
            "      Just x  -> Just (add x 1)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "For Loop 6";
        const char* test_source = ""
            "tenTimes :: Range 10\n"
            "tenTimes = each\n"
            "loopTenTimes :: Maybe UInt\n"
            "loopTenTimes =\n"
            "  loop x = Nothing for i <- tenTimes do\n"
            "    Just 0\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    // {
    //     const char* test_name   = "For Loop 3.5";
    //     const char* test_source = ""
    //         "tenTimes :: Range 10\n"
    //         "tenTimes = each\n"
    //         "loopTenTimes :: (Maybe Int, Maybe Int)\n"
    //         "loopTenTimes = (for tenTimes Nothing loop i x -> Just 0, for tenTimes Nothing loop i y -> Nothing)\n"
    //         "main :: *World -> *World\n"
    //         "main w = w\n";
    //     necro_llvm_test_string(test_name, test_source);
    // }

    {
        const char* test_name   = "For Loop 2.5";
        const char* test_source = ""
            "tenTimes :: Range 10\n"
            "tenTimes = each\n"
            "twentyTimes :: Range 20\n"
            "twentyTimes = each\n"
            "loopTenTimes :: Maybe (Int, Int)\n"
            "loopTenTimes =\n"
            "  loop x1 = Nothing for i1 <- tenTimes do\n"
            "    loop x2 = x1 for i2 <- twentyTimes do\n"
            "      Just (1, 666)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Deep Copy Test 1";
        const char* test_source = ""
            "maybeZero :: Maybe Int -> Int\n"
            "maybeZero m = 0\n"
            "main :: *World -> *World\n"
            "main w = print (maybeZero Nothing) w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Deep Copy Test 3";
        const char* test_source = ""
            "maybeZero :: ((Int, Int), (Float, Float)) -> Int\n"
            "maybeZero m = 0\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 1";
        const char* test_source = ""
            "counter :: Int\n"
            "counter = let x ~ 0 = add x 1 in x\n"
            "main :: *World -> *World\n"
            "main w = print counter w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 2";
        const char* test_source = ""
            "counter :: Maybe Int\n"
            "counter = x where\n"
            "  x ~ Nothing = x\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 3";
        const char* test_source = ""
            "counter :: Maybe Int\n"
            "counter = x where\n"
            "  x ~ Nothing =\n"
            "    case x of\n"
            "      Nothing -> Just 0\n"
            "      Just i  -> Just (add i 1)\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case counter of\n"
            "    Nothing -> w\n"
            "    Just i  -> print i w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 1.5";
        const char* test_source = ""
            "counter :: Int\n"
            "counter =\n"
            "  case (gt mouseX 50) of\n"
            "    True  -> let x ~ 0 = (add x 1) in x\n"
            "    False -> let y ~ 0 = (sub y 1) in y\n"
            "main :: *World -> *World\n"
            "main w = print counter w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 3.1";
        const char* test_source = ""
            "counter :: ((Int, Int), (Int, Int))\n"
            "counter = x where\n"
            "  x ~ ((0, 1), (2, 3)) =\n"
            "    case (gt mouseX 50, x) of\n"
            "      (True,  ((x, y), r)) -> (r, (y, x))\n"
            "      (False, ((z, w), r)) -> ((w, z), r)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 1";
        const char* test_source = ""
            "iopviously :: Int\n"
            "iopviously = add 1 0\n"
            "uopviously :: UInt\n"
            "uopviously = add 1 0\n"
            "fopviously :: Float\n"
            "fopviously = add 1 0\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 2";
        const char* test_source = ""
            "iopviously :: Int\n"
            "iopviously = sub 1 0\n"
            "uopviously :: UInt\n"
            "uopviously = sub 1 0\n"
            "fopviously :: Float\n"
            "fopviously = sub 1 0\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 3";
        const char* test_source = ""
            "iopviously :: Int\n"
            "iopviously = mul 1 0\n"
            "uopviously :: UInt\n"
            "uopviously = mul 1 0\n"
            "fopviously :: Float\n"
            "fopviously = mul 1 0\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 4";
        const char* test_source = ""
            "iopviously :: Int\n"
            "iopviously = abs 1 \n"
            "uopviously :: UInt\n"
            "uopviously = abs 1 \n"
            "fopviously :: Float\n"
            "fopviously = abs 1.0 \n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 5";
        const char* test_source = ""
            "iopviously :: Int\n"
            "iopviously = signum 100 \n"
            "uopviously :: UInt\n"
            "uopviously = signum 100 \n"
            "fopviously :: Float\n"
            "fopviously = signum 100.0 \n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 6";
        const char* test_source = ""
            "iopviously :: Int\n"
            "iopviously = fromInt 1 \n"
            "uopviously :: UInt\n"
            "uopviously = fromInt 1 \n"
            "fopviously :: Float\n"
            "fopviously = fromInt 1 \n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 7";
        const char* test_source = ""
            "iopviously :: Int -> Bool\n"
            "iopviously x = eq x x\n"
            "uopviously :: UInt -> Bool\n"
            "uopviously x = eq x x\n"
            "fopviously :: Float -> Bool\n"
            "fopviously x = eq x x\n"
            "nopviously :: () -> Bool\n"
            "nopviously x = eq x x\n"
            "bopviously :: Bool -> Bool\n"
            "bopviously x = eq x x\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 8";
        const char* test_source = ""
            "iopviously :: Int -> Bool\n"
            "iopviously x = neq x x\n"
            "uopviously :: UInt -> Bool\n"
            "uopviously x = neq x x\n"
            "fopviously :: Float -> Bool\n"
            "fopviously x = neq x x\n"
            "nopviously :: () -> Bool\n"
            "nopviously x = neq x x\n"
            "bopviously :: Bool -> Bool\n"
            "bopviously x = neq x x\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 9";
        const char* test_source = ""
            "iopviously :: Int -> Bool\n"
            "iopviously x = lt x x\n"
            "uopviously :: UInt -> Bool\n"
            "uopviously x = lt x x\n"
            "fopviously :: Float -> Bool\n"
            "fopviously x = lt x x\n"
            "nopviously :: () -> Bool\n"
            "nopviously x = lt x x\n"
            "bopviously :: Bool -> Bool\n"
            "bopviously x = lt x x\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 10";
        const char* test_source = ""
            "iopviously :: Int -> Bool\n"
            "iopviously x = gt x x\n"
            "uopviously :: UInt -> Bool\n"
            "uopviously x = gt x x\n"
            "fopviously :: Float -> Bool\n"
            "fopviously x = gt x x\n"
            "nopviously :: () -> Bool\n"
            "nopviously x = gt x x\n"
            "bopviously :: Bool -> Bool\n"
            "bopviously x = gt x x\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 11";
        const char* test_source = ""
            "iopviously :: Int -> Bool\n"
            "iopviously x = gte x x\n"
            "uopviously :: UInt -> Bool\n"
            "uopviously x = gte x x\n"
            "fopviously :: Float -> Bool\n"
            "fopviously x = gte x x\n"
            "nopviously :: () -> Bool\n"
            "nopviously x = gte x x\n"
            "bopviously :: Bool -> Bool\n"
            "bopviously x = gte x x\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 12";
        const char* test_source = ""
            "iopviously :: Int -> Bool\n"
            "iopviously x = lte x x\n"
            "uopviously :: UInt -> Bool\n"
            "uopviously x = lte x x\n"
            "fopviously :: Float -> Bool\n"
            "fopviously x = lte x x\n"
            "nopviously :: () -> Bool\n"
            "nopviously x = lte x x\n"
            "bopviously :: Bool -> Bool\n"
            "bopviously x = lte x x\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "PrimOp 13";
        const char* test_source = ""
            "andviously :: Bool\n"
            "andviously = True && False\n"
            "orviously :: Bool\n"
            "orviously = True || False\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Unboxed Type Function Parameter";
        const char* test_source = ""
            "dropIt :: (#Int, Bool, Float#) -> (#Int, Bool, Float#)\n"
            "dropIt x = x\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Unboxed Type Con";
        const char* test_source = ""
            "bopIt :: Int -> (#Int, Bool, Maybe Float#)\n"
            "bopIt i = (#i, False, Nothing#)\n"
            "gotIt :: (#Int, Bool, Maybe Float#)\n"
            "gotIt = bopIt 440\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Unboxed Type Con 2";
        const char* test_source = ""
            "bopIt :: Int -> (#Int, Bool, (#(), Maybe Float#)#)\n"
            "bopIt i = (#i, False, (#(), Nothing#)#)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Unboxed Embedded";
        const char* test_source = ""
            "stopIt :: Maybe (#Int, Float#)\n"
            "stopIt = Just (#333, 44.4#)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Unboxed Case 1";
        const char* test_source = ""
            "stopIt :: Int\n"
            "stopIt =\n"
            "  case (#0, False#) of\n"
            "    (#i, b#) -> i\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Unboxed Case 2";
        const char* test_source = ""
            "muddyTheWaters :: Int -> Int\n"
            "muddyTheWaters i =\n"
            "  case (#i, False#) of\n"
            "    (#i2, b#) -> i2\n"
            "main :: *World -> *World\n"
            "main w = print (muddyTheWaters 22) w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Unboxed Case 3";
        const char* test_source = ""
            "manInTheBox :: (#Int, Int#)\n"
            "manInTheBox = (#666, 777#)\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case manInTheBox of\n"
            "    (#x, y#) -> print y (print x w)\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Unboxed Case 4";
        const char* test_source = ""
            "muddyTheWaters :: Int -> (#Int, Int, Maybe ()#)\n"
            "muddyTheWaters i = (#i, i * 2, Nothing#)\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case muddyTheWaters 333 of\n"
            "    (#x, y, _#) -> print x (print y w)\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Unboxed Case 5";
        const char* test_source = ""
            "muddyTheWaters :: Int -> (#Bool, (#Bool, Int#)#)\n"
            "muddyTheWaters i = (#True, (#False, i * 2#)#)\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case muddyTheWaters 333 of\n"
            "    (#_, (#_, i#)#) -> print i w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Unboxed Case 6";
        const char* test_source = ""
            "muddyTheWaters :: Int -> Maybe (#Bool, Int#)\n"
            "muddyTheWaters i = Just (#False, i * 2#)\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case muddyTheWaters 333 of\n"
            "    Just (#_, i#) -> print i w\n"
            "    _             -> w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Unboxed Case 7";
        const char* test_source = ""
            "tenTimes :: Range 10\n"
            "tenTimes = each\n"
            "forWhat :: Int -> (#Int, Int#)\n"
            "forWhat x =\n"
            "  loop w = (#x, 1#) for i <- tenTimes do\n"
            "    case w of\n"
            "        (#xi, yi#) -> (#xi * yi, yi + 1#)\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case forWhat 33 of\n"
            "    (#x, _#) -> print x w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "I64 1";
        const char* test_source = ""
            "commodore64 :: I64 -> I64\n"
            "commodore64 x = x * 2\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "I64 2";
        const char* test_source = ""
            "commodore64 :: Int -> I64\n"
            "commodore64 x = fromInt x * 2\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "F64 1";
        const char* test_source = ""
            "commodore64 :: F64 -> F64\n"
            "commodore64 x = x * 2 * 3.0\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "F64 2";
        const char* test_source = ""
            "commodore64 :: Float -> F64\n"
            "commodore64 x = fromFloat x * 2 * 3.0\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Struct64 1";
        const char* test_source = ""
            "data Commodore64 = Commodore64 I64 I64\n"
            "commodore64 :: Commodore64 -> Commodore64\n"
            "commodore64 c =\n"
            "  case c of\n"
            "    Commodore64 x y -> let z = x * y in Commodore64 z z\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Struct64 2";
        const char* test_source = ""
            "data Commodore64 = Commodore64 F64 F64\n"
            "commodore64 :: Commodore64 -> Commodore64\n"
            "commodore64 c =\n"
            "  case c of\n"
            "    Commodore64 x y -> let z = x * y in Commodore64 z z\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Array 5";
        const char* test_source = ""
            "somethingInThere :: Array 33 Int\n"
            "somethingInThere =\n"
            "  freezeArray <| loop a =  unsafeEmptyArray () for i <- each do\n"
            "    writeArray i 22 a\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Audio 1";
        const char* test_source = ""
            "coolSaw :: Mono Audio\n"
            "coolSaw = saw 440\n"
            "main :: *World -> *World\n"
            "main w = outAudio 0 coolSaw w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Main 1";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = print (1 + 2) w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Seq 7";
        const char* test_source = ""
            "coolSeq :: Seq Int\n"
            "coolSeq = 666 * 22 + 3 * 4 - 256 * 10\n"
            "seqGo :: SeqValue Int\n"
            "seqGo = runSeq coolSeq ()\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Print Char";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = printLn 'x' w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Print Rational";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = printLn (1 // 4) w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Seq 7";
        const char* test_source = ""
            "coolSeq :: Seq Int\n"
            "coolSeq = 666 * 22 + 3 * 4 - 256 * 10\n"
            "seqGo :: SeqValue Int\n"
            "seqGo = runSeq coolSeq ()\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "If then If Then else then doom then doom";
        const char* test_source = ""
            "ifTest :: Bool -> Bool\n"
            "ifTest t = if t then (if False then True else False) else (if False then True else False)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Print Seq";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = print [11 22 _ <4 5 6>] w\n";
        necro_llvm_test_string(test_name, test_source);
    }

/*

*/

}

void necro_llvm_test_jit()
{

/*

    {
        const char* test_name   = "Main 0";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Main 1";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = print (add 1 2) w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 6";
        const char* test_source = ""
            "counter :: Int\n"
            "counter ~ 0 = add counter 1\n"
            "main :: *World -> *World\n"
            "main w = print counter w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Data 1";
        const char* test_source = ""
            "data TwoInts = TwoInts Int Int\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 2";
        const char* test_source = ""
            "data SomeOrNone = Some Int | None\n"
            "main w =\n"
            "  case None of\n"
            "    None -> print 0 w\n"
            "    _    -> print 1 w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 9";
        const char* test_source = ""
            "data TwoForOne = One Int | Two Int Int\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case One 666 of\n"
            "    One x   -> print x w\n"
            "    Two y z -> print z (print y w)\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 10";
        const char* test_source = ""
            "data TwoForOne = One Int | Two Int Int\n"
            "data MaybeTwoForOne = JustTwoForOne TwoForOne | NoneForOne\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case JustTwoForOne (One mouseX) of\n"
            "    NoneForOne              -> w\n"
            "    JustTwoForOne (One x)   -> print x w\n"
            "    JustTwoForOne (Two y z) -> print z (print y w)\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 11";
        const char* test_source = ""
            "data OneOrZero    = One Int | Zero\n"
            "data MoreThanZero = MoreThanZero OneOrZero OneOrZero\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case MoreThanZero (One 3) Zero of\n"
            "    MoreThanZero (One i1)  (One i2) -> print i2 (print i1 w)\n"
            "    MoreThanZero Zero      (One i3) -> print i3 w\n"
            "    MoreThanZero (One i4)  Zero     -> print (mul i4 11) w\n"
            "    MoreThanZero Zero      Zero     -> w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case 12";
        const char* test_source = ""
            "data TwoForOne  = One Int | Two Int Int\n"
            "data FourForOne = MoreThanZero TwoForOne TwoForOne | Zero\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case MoreThanZero (Two 1 2) (Two 3 4) of\n"
            "    Zero                                    -> w\n"
            "    MoreThanZero (One i1)     (One i2)      -> print i2 (print i1 w)\n"
            "    MoreThanZero (Two i3 i4)  (One i5)      -> print i5 (print i4 (print i3 w))\n"
            "    MoreThanZero (One i6)     (Two i7  i8)  -> print i8 (print i7 (print i6 w))\n"
            "    MoreThanZero (Two i9 i10) (Two i11 i12) -> print (add i9 (add i10 (add i11 i12))) w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Poly 5";
        const char* test_source = ""
            "nothingFromSomething :: Int -> (Maybe Float, Maybe Int)\n"
            "nothingFromSomething i = (Nothing, Just i)\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case nothingFromSomething 22 of\n"
            "    (_, Just i) -> print i w\n"
            "    _           -> print -1 w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 1";
        const char* test_source = ""
            "main w =\n"
            "  case 0 of\n"
            "    0 -> print 0 w\n"
            "    1 -> print 1 w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 2";
        const char* test_source = ""
            "main w =\n"
            "  case False of\n"
            "    False -> print 0 w\n"
            "    True  -> print 1 w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 4";
        const char* test_source = ""
            "basketCase :: Int -> *World -> *World\n"
            "basketCase i w =\n"
            "  case i of\n"
            "    0 -> print 666 w\n"
            "    u -> print u w\n"
            "main :: *World -> *World\n"
            "main w = basketCase mouseX w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 5";
        const char* test_source = ""
            "basketCase :: Int -> *World -> *World\n"
            "basketCase i w =\n"
            "  case i of\n"
            "    u -> print u w\n"
            "main :: *World -> *World\n"
            "main w = basketCase 0 w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 6";
        const char* test_source = ""
            "basketCase :: Maybe Int -> *World -> *World\n"
            "basketCase m w =\n"
            "  case m of\n"
            "    Nothing -> print 100 w\n"
            "    Just 0  -> print 200 w\n"
            "    Just -1 -> print 300 w\n"
            "    Just i  -> print i w\n"
            "main :: *World -> *World\n"
            "main w = basketCase (Just -22) w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 7";
        const char* test_source = ""
            "basketCase :: Int -> *World -> *World\n"
            "basketCase i w =\n"
            "  case gt i 50 of\n"
            "    True -> print 666 w\n"
            "    _    -> print 777 w\n"
            "main :: *World -> *World\n"
            "main w = basketCase mouseX w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 9";
        const char* test_source = ""
            "basketCase :: Int -> *World -> *World\n"
            "basketCase i w =\n"
            "  case Just (gt i 50) of\n"
            "    Nothing    -> print 0 w\n"
            "    Just True  -> print 666 w\n"
            "    Just False -> print 777 w\n"
            "main :: *World -> *World\n"
            "main w = basketCase mouseX w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Case Lit 10";
        const char* test_source = ""
            "basketCase :: Int -> *World -> *World\n"
            "basketCase i w =\n"
            "  case Just (gt i 50) of\n"
            "    Nothing    -> print 0 w\n"
            "    Just False -> print 777 w\n"
            "    Just _     -> print 666 w\n"
            "main :: *World -> *World\n"
            "main w = basketCase mouseX w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "For Loop 1";
        const char* test_source = ""
            "tenTimes :: Range 10\n"
            "tenTimes = each\n"
            "loopTenTimes :: Int\n"
            "loopTenTimes =\n"
            "  loop x = mouseX for i <- tenTimes do\n"
            "    mul x 2\n"
            "main :: *World -> *World\n"
            "main w = print loopTenTimes w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "For Loop 2";
        const char* test_source = ""
            "tenTimes :: Range 10\n"
            "tenTimes = each\n"
            "loopTenTimes :: Int\n"
            "loopTenTimes =\n"
            "  loop x1 = 0 for i1 = tenTimes do\n"
            "    loop x2 = x1 for i2 <- tenTimes do\n"
            "      add x2 mouseX\n"
            "main :: *World -> *World\n"
            "main w = print loopTenTimes w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "For Loop 4";
        const char* test_source = ""
            "tenTimes :: Range 10\n"
            "tenTimes = each\n"
            "loopTenTimes :: Maybe Int -> Int\n"
            "loopTenTimes m =\n"
            "  case m of\n"
            "    Nothing ->\n
            "      loop x = 0 for i <- tenTimes do\n"
            "        add x 1\n"
            "    Just y  ->\n"
            "      loop x = 0 for i <- tenTimes do\n"
            "        add x y\n"
            "main :: *World -> *World\n"
            "main w = print (loopTenTimes (Just mouseX)) w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "For Loop 5";
        const char* test_source = ""
            "tenTimes :: Range 10\n"
            "tenTimes = each\n"
            "loopTenTimes :: Maybe Int -> Maybe Int\n"
            "loopTenTimes m =\n"
            "  loop x = m for i <- tenTimes do\n"
            "    case x of\n"
            "      Nothing -> Just 0\n"
            "      Just x  -> Just (add x 1)\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case loopTenTimes (Just mouseX) of\n"
            "    Nothing -> w\n"
            "    Just i  -> print i w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 1";
        const char* test_source = ""
            "counter :: Int\n"
            "counter = let x ~ 0 = add x 1 in x\n"
            "main :: *World -> *World\n"
            "main w = print counter w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 3";
        const char* test_source = ""
            "counter :: Maybe Int\n"
            "counter = x where\n"
            "  x ~ Nothing =\n"
            "    case x of\n"
            "      Nothing -> Just 0\n"
            "      Just i  -> Just (add i 1)\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case counter of\n"
            "    Nothing -> w\n"
            "    Just i  -> print i w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 1.1";
        const char* test_source = ""
            "counter :: Int\n"
            "counter =\n"
            "  case (gt mouseX 50) of\n"
            "    True  -> let x ~ 0 = add x 1 in x\n"
            "    False -> let y ~ 0 = sub y 1 in y\n"
            "main :: *World -> *World\n"
            "main w = print counter w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 5.5";
        const char* test_source = ""
            "counter :: (Int, Int)\n"
            "counter = x where\n"
            "  x ~ (0, 1) =\n"
            "    case x of\n"
            "      (xl, xr) -> (xr, xl)\n"
            "  y ~ (2, 3) =\n"
            "    case y of\n"
            "      (yl, yr) -> (yr, yl)\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case counter of\n"
            "    (xl, _) -> print xl w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Unboxed Case 4";
        const char* test_source = ""
            "muddyTheWaters :: Int -> (#Int, Int, Maybe ()#)\n"
            "muddyTheWaters i = (#i, i * 2, Nothing#)\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case muddyTheWaters 333 of\n"
            "    (#x, y, _#) -> print x (print y w)\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Audio 1";
        const char* test_source = ""
            "coolSaw :: Mono Audio\n"
            "coolSaw = sawOsc 440\n"
            "main :: *World -> *World\n"
            "main w = outAudio 0 coolSaw w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Main 1";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = print 666 w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Floored 1";
        const char* test_source = ""
            "floored :: F64\n"
            "floored ~ 0 = floored + 0.05\n"
            "main :: *World -> *World\n"
            "main w = print (fastFloor floored) w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Floored 2";
        const char* test_source = ""
            "fcounter :: F64\n"
            "fcounter ~ 0 = x\n"
            "  where\n"
            "    acc = fcounter + 0.00514123123\n"
            "    x   = acc - fastFloor acc\n"
            "main :: *World -> *World\n"
            "main w = print fcounter w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Floored 3";
        const char* test_source = ""
            "sawAccTest :: F64\n"
            "sawAccTest ~ 0 =\n"
            "  case sawOut of\n"
            "    (#acc, _#) -> acc\n"
            "  where\n"
            "    sawOut = accumulateSaw 10.0 sawAccTest\n"
            "main :: *World -> *World\n"
            "main w = print sawAccTest w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "AudioOut 2";
        const char* test_source = ""
            "sawTest :: Stereo Audio\n"
            "sawTest = pan (fromInt mouseX / 150) (sawOsc 440)\n"
            "main :: *World -> *World\n"
            "main w = outAudio 0 sawTest w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Audio 1";
        const char* test_source = ""
            "coolSaw :: Mono Audio\n"
            "coolSaw = saw (saw 0.05 * 3980 + 4000) * 0.25\n"
            "stereoSaw :: Stereo\n"
            "stereoSaw = stereo coolSaw coolSaw\n"
            "main :: *World -> *World\n"
            "main w = outAudio 0 stereoSaw w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Audio 2";
        const char* test_source = ""
            "coolSaw :: Mono Audio\n"
            "coolSaw = saw (saw 0.1 * 750 + 1000 + saw (saw 0.22 * 10 + 15) * (saw 0.15 * 60 + 240)) * 0.25\n"
            "stereoSaw :: Stereo\n"
            "stereoSaw = stereo coolSaw coolSaw\n"
            "main :: *World -> *World\n"
            "main w = outAudio 0 stereoSaw w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Print Char";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = printLn 'x' w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Print Rational 1";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = printLn (1 % 4) w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Print Rational 2";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = printLn (mouseX % 4) (print '=' (print mouseX w))\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Print Rational 3";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = printLn ((1 % 2) + (1 % 4) + (1 % 4)) w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Print Rational 4";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = printLn ((3 % 4) * (1 % 2) * (1 % 3)) w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Print Rational 5";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = printLn ((2 % 3) - ((1 % 4) * (1 % 2))) w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Print Rational 6";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = printLn (max (1 % 256) (42 % 32)) w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Print Rational 7";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = printLn ((10 % 1) - (20 % 2)) w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Print Rational 8";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = printLn ((10 % 1) - (20 % 1)) w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Print Seq 1";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = print [11 <22 25> _ <4 5 [333:444:555:666]>] w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Print Seq 2";
        const char* test_source = ""
            "twoAgainstFour :: Seq Int\n"
            "twoAgainstFour = <2 3> + <4 5 6 7>\n"
            "main :: *World -> *World\n"
            "main w = print twoAgainstFour w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Print Seq 2";
        const char* test_source = ""
            "twoAgainstThree :: Seq Int\n"
            "twoAgainstThree = <2 3> + <4 5 6>\n"
            "main :: *World -> *World\n"
            "main w = print twoAgainstThree w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Print Seq 3";
        const char* test_source = ""
            "twoAgainstThree :: Seq Int\n"
            "twoAgainstThree = <2 3> @+ <4 5 6>\n"
            "main :: *World -> *World\n"
            "main w = print twoAgainstThree w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Print Seq 4";
        const char* test_source = ""
            "twoAgainstThree :: Seq Int\n"
            "twoAgainstThree = <2 3> +@ <4 5 6>\n"
            "main :: *World -> *World\n"
            "main w = print twoAgainstThree w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Print Seq 5";
        const char* test_source = ""
            "twoAgainstThree :: Seq Int\n"
            "twoAgainstThree = fromInt mouseX <@ [<0 1> <4 _ 6>]\n"
            "main :: *World -> *World\n"
            "main w = print twoAgainstThree w\n";
        necro_llvm_jit_string(test_name, test_source);
    }


*/

    // {
    //     const char* test_name   = "Add";
    //     const char* test_source = ""
    //         "main :: *World -> *World\n"
    //         "main w = testAssertion (2 + 2 == 4) w\n";
    //     necro_llvm_jit_string(test_name, test_source);
    // }

    // {
    //     const char* test_name   = "Mod";
    //     const char* test_source = ""
    //         "main :: *World -> *World\n"
    //         "main w = testAssertion (4 % 3 == 0) w\n";
    //     necro_llvm_jit_string(test_name, test_source);
    // }

    // {
    //     const char* test_name   = "GCD";
    //     const char* test_source = ""
    //         "main :: *World -> *World\n"
    //         "main w = testAssertion (1 // 2 == 4 // 8) w\n";
    //     necro_llvm_jit_string(test_name, test_source);
    // }

    // {
    //     const char* test_name   = "Abs 1";
    //     const char* test_source = ""
    //         "main :: *World -> *World\n"
    //         "main w = testAssertion (abs -33 == 33) w\n";
    //     necro_llvm_jit_string(test_name, test_source);
    // }

    // {
    //     const char* test_name   = "Abs 2";
    //     const char* test_source = ""
    //         "main :: *World -> *World\n"
    //         "main w = testAssertion (abs 667 == 667) w\n";
    //     necro_llvm_jit_string(test_name, test_source);
    // }

    // {
    //     const char* test_name   = "Signum 1";
    //     const char* test_source = ""
    //         "main :: *World -> *World\n"
    //         "main w = testAssertion (signum -666 == -1) w\n";
    //     necro_llvm_jit_string(test_name, test_source);
    // }

    // {
    //     const char* test_name   = "Signum 2";
    //     const char* test_source = ""
    //         "main :: *World -> *World\n"
    //         "main w = testAssertion (signum 334 == 1) w\n";
    //     necro_llvm_jit_string(test_name, test_source);
    // }

    // {
    //     const char* test_name   = "Signum Float 1";
    //     const char* test_source = ""
    //         "main :: *World -> *World\n"
    //         "main w = testAssertion (signum -4.60234 == -1) w\n";
    //     necro_llvm_jit_string(test_name, test_source);
    // }

    // {
    //     const char* test_name   = "Signum Float 2";
    //     const char* test_source = ""
    //         "main :: *World -> *World\n"
    //         "main w = testAssertion (signum 9.87654321 == 1) w\n";
    //     necro_llvm_jit_string(test_name, test_source);
    // }

    // {
    //     const char* test_name   = "FAbs 1";
    //     const char* test_source = ""
    //         "main :: *World -> *World\n"
    //         "main w = testAssertion (abs -11.54321 == 11.54321) w\n";
    //     necro_llvm_jit_string(test_name, test_source);
    // }

    // {
    //     const char* test_name   = "FAbs 2";
    //     const char* test_source = ""
    //         "main :: *World -> *World\n"
    //         "main w = testAssertion (abs 10.12345 == 10.12345) w\n";
    //     necro_llvm_jit_string(test_name, test_source);
    // }

    // {
    //     const char* test_name   = "FAbs 3";
    //     const char* test_source = ""
    //         "main :: *World -> *World\n"
    //         "main w = testAssertion (abs (negate pi) == pi) w\n";
    //     necro_llvm_jit_string(test_name, test_source);
    // }

    // {
    //     const char* test_name   = "Negative Rational";
    //     const char* test_source = ""
    //         "main :: *World -> *World\n"
    //         "main w = testAssertion (1 // -2 == -2 // 4) w\n";
    //     necro_llvm_jit_string(test_name, test_source);
    // }

    // {
    //     const char* test_name   = "GT Rational";
    //     const char* test_source = ""
    //         "main :: *World -> *World\n"
    //         "main w = testAssertion (3 // 4 > 4 // 8) w\n";
    //     necro_llvm_jit_string(test_name, test_source);
    // }

    // {
    //     const char* test_name   = "LT Rational";
    //     const char* test_source = ""
    //         "main :: *World -> *World\n"
    //         "main w = testAssertion (6 // 12 < 7 // 9) w\n";
    //     necro_llvm_jit_string(test_name, test_source);
    // }

    // {
    //     const char* test_name   = "Bit And 1";
    //     const char* test_source = ""
    //         "main :: *World -> *World\n"
    //         "main w = testAssertion (bitAnd 12 4 == 4) w\n";
    //     necro_llvm_jit_string(test_name, test_source);
    // }

    // {
    //     const char* test_name   = "Bit And 2";
    //     const char* test_source = ""
    //         "main :: *World -> *World\n"
    //         "main w = testAssertion (bitAnd 12 5 == 4) w\n";
    //     necro_llvm_jit_string(test_name, test_source);
    // }

    // {
    //     const char* test_name   = "bitOr 1";
    //     const char* test_source = ""
    //         "main :: *World -> *World\n"
    //         "main w = testAssertion (bitOr 8 4 == 12) w\n";
    //     necro_llvm_jit_string(test_name, test_source);
    // }

    // {
    //     const char* test_name   = "bitOr 2";
    //     const char* test_source = ""
    //         "main :: *World -> *World\n"
    //         "main w = testAssertion (bitOr 9 5 == 13) w\n";
    //     necro_llvm_jit_string(test_name, test_source);
    // }

    // {
    //     const char* test_name   = "bitShiftLeft 1";
    //     const char* test_source = ""
    //         "main :: *World -> *World\n"
    //         "main w = testAssertion (bitShiftLeft 2 3 == 16) w\n";
    //     necro_llvm_jit_string(test_name, test_source);
    // }

    // {
    //     const char* test_name   = "bitShiftLeft 2";
    //     const char* test_source = ""
    //         "main :: *World -> *World\n"
    //         "main w = testAssertion (bitShiftLeft 1 3 == 8) w\n";
    //     necro_llvm_jit_string(test_name, test_source);
    // }

    // {
    //     const char* test_name   = "bitShiftLeft 3";
    //     const char* test_source = ""
    //         "main :: *World -> *World\n"
    //         "main w = testAssertion (bitShiftLeft 3 2 == 12) w\n";
    //     necro_llvm_jit_string(test_name, test_source);
    // }

    // {
    //     const char* test_name   = "bitShiftRight 1";
    //     const char* test_source = ""
    //         "main :: *World -> *World\n"
    //         "main w = testAssertion (bitShiftRight 8 2 == 2) w\n";
    //     necro_llvm_jit_string(test_name, test_source);
    // }

    /* { */
    /*     const char* test_name   = "bitShiftRight 2"; */
    /*     const char* test_source = "" */
    /*         "main :: *World -> *World\n" */
    /*         "main w = testAssertion (bitShiftRight 9 2 == 2) w\n"; */
    /*     necro_llvm_jit_string(test_name, test_source); */
    /* } */

    // {
    //     const char* test_name   = "Print Complex";
    //     const char* test_source = ""
    //         "imVeryComplex :: Complex\n"
    //         "imVeryComplex = 777\n"
    //         "main :: *World -> *World\n"
    //         "main w = printLn imVeryComplex w\n";
    //     necro_llvm_jit_string(test_name, test_source);
    // }

    {
        const char* test_name   = "Print Audio Block";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = testJit w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    /* { */
    /*     const char* test_name   = "Print Saw"; */
    /*     const char* test_source = "" */
    /*         "coolSaw :: Stereo Audio\n" */
    /*         "coolSaw = saw 440 * 0.25 |> perc 2 5 1\n" */
    /*         "main :: *World -> *World\n" */
    /*         "main w = outAudio 0 coolSaw w\n"; */
    /*     necro_llvm_jit_string(test_name, test_source); */
    /* } */

    // {
    //     const char* test_name   = "bitReverse 1";
    //     const char* test_source = ""
    //         "main :: *World -> *World\n"
    //         "main w = printLn (bitReverse 24) w\n";
    //     necro_llvm_jit_string(test_name, test_source);
    // }

    // TODO: Test divide by zero

/*

    {
        const char* test_name   = "Mouse 1";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = printLn mouseX w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Audio 3";
        const char* test_source = ""
            "coolSaw :: Mono Audio\n"
            "coolSaw = (saw 300 + saw (saw 0.05 * 1940 + 2000)) * 0.125\n"
            "stereoSaw :: Stereo\n"
            "stereoSaw = stereo coolSaw coolSaw\n"
            "main :: *World -> *World\n"
            "main w = outAudio 0 stereoSaw w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Audio 4 - Stereo Pan";
        const char* test_source = ""
            "coolSaw :: Stereo Audio\n"
            "coolSaw = pan (fromInt mouseX / 150) (stereo (saw 440) (saw 880))\n"
            "main :: *World -> *World\n"
            "main w = outAudio 0 coolSaw w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

    {
        const char* test_name   = "Audio 4";
        const char* test_source = ""
            "coolSaw :: Mono Audio\n"
            "coolSaw = saw (fromInt mouseX * 4) * 0.25\n"
            "stereoSaw :: Stereo\n"
            "stereoSaw = stereo coolSaw coolSaw\n"
            "main :: *World -> *World\n"
            "main w = outAudio 0 stereoSaw w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

// TODO / NOTE: This should parse but isn't?
    {
        const char* test_name   = "Rec 1.2";
        const char* test_source = ""
            "counter :: (Int, Int)\n"
            "counter =\n"
            "  case gt mouseX 50 of\n"
            "    True ->\n"
            "        let x ~ (0, 1) =\n"
            "            case x of\n"
            "               (xa, xd) -> (add xa xd, xd)\n"
            "        in x\n"
            "    False ->\n"
            "        let y ~ (0, 1) =\n"
            "            case y of\n"
            "               (ya, yd) -> (sub ya yd, yd)\n"
            "        in y\n"
            "main :: *World -> *World\n"
            "main w =\n"
            "  case counter of\n"
            "    (cval, _) -> print cval w\n";
        necro_llvm_jit_string(test_name, test_source);
    }

*/

}

void necro_llvm_test_compile()
{
/*

    {
        const char* test_name   = "Main 1";
        const char* test_source = ""
            "main :: *World -> *World\n"
            // "main w = print 666 w\n";
            "main w = w\n";
        necro_llvm_compile_string(test_name, test_source);
    }

    {
        const char* test_name   = "Floored 1";
        const char* test_source = ""
            "floored :: F64\n"
            "floored ~ 0 = floored + 0.01\n"
            "main :: *World -> *World\n"
            "main w = print (fastFloor floored) w\n";
        necro_llvm_compile_string(test_name, test_source);
    }

    // defunc null type here!? Why?
    {
        const char* test_name   = "Floored 2";
        const char* test_source = ""
            "fcounter :: F64\n"
            "fcounter ~ 0 = x\n"
            "  where\n"
            "    acc = fcounter + 0.0514123123\n"
            "    x   = acc - fastFloor acc\n"
            "main :: *World -> *World\n"
            "main w = print fcounter w\n";
        necro_llvm_compile_string(test_name, test_source);
    }

*/

    {
        const char* test_name   = "Audio 1";
        const char* test_source = ""
            "coolSaw :: Mono Audio\n"
            "coolSaw = saw 440\n"
            "main :: *World -> *World\n"
            "main w = outAudio 0 coolSaw w\n";
        necro_llvm_compile_string(test_name, test_source);
    }

}

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdint.h>
#include <inttypes.h>

#include "mach_print.h"
#include "mach_ast.h"
#include "utility.h"


///////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////
void necro_mach_print_ast_go(struct NecroMachAst* ast, size_t depth);

///////////////////////////////////////////////////////
// Printing
///////////////////////////////////////////////////////
void necro_mach_print_fn(NecroMachAst* ast, size_t depth)
{
    assert(ast->type == NECRO_MACH_FN_DEF);
    print_white_space(depth);
    if (ast->fn_def.fn_type == NECRO_MACH_FN_RUNTIME)
        printf("foreign ");
    printf("fn %s(", ast->fn_def.symbol->name->str);
    for (size_t i = 0; i < ast->necro_machine_type->fn_type.num_parameters; ++i)
    {
        necro_mach_type_print_go(ast->necro_machine_type->fn_type.parameters[i], false);
        if (i < ast->necro_machine_type->fn_type.num_parameters - 1)
            printf(", ");
    }
    printf(") -> ");
    necro_mach_type_print_go(ast->necro_machine_type->fn_type.return_type, false);
    if (ast->fn_def.fn_type == NECRO_MACH_FN_RUNTIME)
    {
        printf("\n");
        return;
    }
    printf("\n");
    print_white_space(depth);
    printf("{\n");
    necro_mach_print_ast_go(ast->fn_def.call_body, depth + 4);
    print_white_space(depth);
    printf("}\n");
}

typedef enum
{
     NECRO_PRINT_VALUE_TYPE,
     NECRO_DONT_PRINT_VALUE_TYPE
} NECRO_SHOULD_PRINT_VALUE_TYPE;

void necro_mach_print_value(NecroMachAst* ast, NECRO_SHOULD_PRINT_VALUE_TYPE should_print_value_type)
{
    assert(ast->type == NECRO_MACH_VALUE);
    NecroMachValue value = ast->value;
    switch (value.value_type)
    {
    case NECRO_MACH_VALUE_VOID:
        break;
    case NECRO_MACH_VALUE_GLOBAL:
        printf("@%s", value.global_symbol->name->str);
        break;
    case NECRO_MACH_VALUE_REG:
        printf("%%%s", value.reg_symbol->name->str);
        break;
    case NECRO_MACH_VALUE_PARAM:
        printf("%%%zu", value.param_reg.param_num);
        break;
    case NECRO_MACH_VALUE_UINT1_LITERAL:
        printf("%uu1", value.uint1_literal);
        return;
    case NECRO_MACH_VALUE_UINT8_LITERAL:
        printf("%uu8", value.uint8_literal);
        return;
    case NECRO_MACH_VALUE_UINT16_LITERAL:
        printf("%uu16", value.uint16_literal);
        return;
    case NECRO_MACH_VALUE_UINT32_LITERAL:
        printf("%uu32", value.uint32_literal);
        return;
    case NECRO_MACH_VALUE_UINT64_LITERAL:
        printf("%" PRIu64 "", value.uint64_literal);
        return;
    case NECRO_MACH_VALUE_INT32_LITERAL:
        printf("%di32", value.int32_literal);
        return;
    case NECRO_MACH_VALUE_INT64_LITERAL:
        printf("%" PRId64 "", value.int64_literal);
        return;
    case NECRO_MACH_VALUE_F32_LITERAL:
        printf("%ff32", value.f32_literal);
        return;
    case NECRO_MACH_VALUE_F64_LITERAL:
        printf("%ff64", value.f64_literal);
        return;
    case NECRO_MACH_VALUE_NULL_PTR_LITERAL:
        printf("null");
        break;
    case NECRO_MACH_VALUE_UNDEFINED:
        printf("undef");
        break;
    default:
        assert(false);
        break;
    }
    if (should_print_value_type == NECRO_PRINT_VALUE_TYPE)
    // Turning this off for now to clear some clutter
    // if (false)
    {
        printf(" (");
        necro_mach_type_print_go(ast->necro_machine_type, false);
        printf(")");
    }
}

void necro_mach_print_block(NecroMachAst* ast, size_t depth)
{
    assert(ast->type == NECRO_MACH_BLOCK);
    print_white_space(depth - 4);
    printf("%s:\n", ast->block.symbol->name->str);
    for (size_t i = 0; i < ast->block.num_statements; ++i)
    {
        necro_mach_print_ast_go(ast->block.statements[i], depth);
        printf("\n");
    }
    assert(ast->block.terminator != NULL);
    switch (ast->block.terminator->type)
    {
    case NECRO_MACH_TERM_RETURN:
        print_white_space(depth);
        printf("return ");
        necro_mach_print_value(ast->block.terminator->return_terminator.return_value, NECRO_PRINT_VALUE_TYPE);
        printf("\n");
        break;
    case NECRO_MACH_TERM_RETURN_VOID:
        print_white_space(depth);
        printf("return void");
        printf("\n");
        break;
    case NECRO_MACH_TERM_SWITCH:
    {
        print_white_space(depth);
        printf("switch ");
        necro_mach_print_value(ast->block.terminator->switch_terminator.choice_val, NECRO_DONT_PRINT_VALUE_TYPE);
        printf(" [");
        NecroMachSwitchList* values = ast->block.terminator->switch_terminator.values;
        assert(values != NULL);
        while (values != NULL)
        {
            printf("%zu: %s, ", values->data.value, values->data.block->block.symbol->name->str);
            values = values->next;
        }
        printf("else: %s]\n", ast->block.terminator->switch_terminator.else_block->block.symbol->name->str);
        break;
    }
    case NECRO_MACH_TERM_BREAK:
        print_white_space(depth);
        printf("break %s\n", ast->block.terminator->break_terminator.block_to_jump_to->block.symbol->name->str);
        break;
    case NECRO_MACH_TERM_COND_BREAK:
        print_white_space(depth);
        printf("condbreak %%%s [true: %s, false: %s]\n",
            ast->block.terminator->cond_break_terminator.cond_value->value.reg_symbol->name->str,
            ast->block.terminator->cond_break_terminator.true_block->block.symbol->name->str,
            ast->block.terminator->cond_break_terminator.false_block->block.symbol->name->str
            );
        break;
    case NECRO_MACH_TERM_UNREACHABLE:
        print_white_space(depth);
        printf("unreachable\n");
        break;
    }
    if (ast->block.next_block != NULL)
    {
        printf("\n");
        necro_mach_print_block(ast->block.next_block, depth);
    }
}

void necro_mach_print_call(NecroMachAst* ast, size_t depth)
{
    UNUSED(depth);
    assert(ast->type == NECRO_MACH_CALL);
    if (ast->call.result_reg->value.value_type == NECRO_MACH_VALUE_VOID)
        printf("call ");
    else
        printf("%%%s = call ", ast->call.result_reg->value.reg_symbol->name->str);
    necro_mach_print_value(ast->call.fn_value, NECRO_DONT_PRINT_VALUE_TYPE);
    printf("(");
    for (size_t i = 0; i < ast->call.num_parameters; ++i)
    {
        necro_mach_print_value(ast->call.parameters[i], NECRO_DONT_PRINT_VALUE_TYPE);
        if (i < ast->call.num_parameters - 1)
            printf(", ");
    }
    printf(")");
}

void necro_mach_print_call_intrinsic(NecroMachAst* ast, size_t depth)
{
    UNUSED(depth);
    assert(ast->type == NECRO_MACH_CALLI);
    if (ast->call_intrinsic.result_reg->value.value_type == NECRO_MACH_VALUE_VOID)
        printf("call ");
    else
        printf("%%%s = call ", ast->call_intrinsic.result_reg->value.reg_symbol->name->str);
    switch (ast->call_intrinsic.intrinsic)
    {
    case NECRO_PRIMOP_INTR_FMA:     printf("fma ");     break;
    case NECRO_PRIMOP_INTR_BREV:    printf("brev ");    break;
    case NECRO_PRIMOP_INTR_FABS:    printf("fabs ");    break;
    case NECRO_PRIMOP_INTR_SIN:     printf("sin ");     break;
    case NECRO_PRIMOP_INTR_COS:     printf("cos ");     break;
    case NECRO_PRIMOP_INTR_EXP:     printf("exp ");     break;
    case NECRO_PRIMOP_INTR_EXP2:    printf("exp2 ");    break;
    case NECRO_PRIMOP_INTR_LOG:     printf("log ");     break;
    case NECRO_PRIMOP_INTR_LOG10:   printf("log10 ");   break;
    case NECRO_PRIMOP_INTR_LOG2:    printf("log2 ");    break;
    case NECRO_PRIMOP_INTR_POW:     printf("pow ");     break;
    case NECRO_PRIMOP_INTR_SQRT:    printf("sqrt ");    break;
    case NECRO_PRIMOP_INTR_FFLR:    printf("floor ");   break;
    case NECRO_PRIMOP_INTR_FCEIL:   printf("fceil ");   break;
    case NECRO_PRIMOP_INTR_FTRNC:   printf("ftrnc ");   break;
    case NECRO_PRIMOP_INTR_FRND:    printf("frnd ");    break;
    case NECRO_PRIMOP_INTR_FCPYSGN: printf("fcpysgn "); break;
    case NECRO_PRIMOP_INTR_FMIN:    printf("fmin ");    break;
    case NECRO_PRIMOP_INTR_FMAX:    printf("fmax ");    break;
    default:                        assert(false);      break;
    }
    printf("(");
    for (size_t i = 0; i < ast->call_intrinsic.num_parameters; ++i)
    {
        necro_mach_print_value(ast->call_intrinsic.parameters[i], NECRO_DONT_PRINT_VALUE_TYPE);
        if (i < ast->call_intrinsic.num_parameters - 1)
            printf(", ");
    }
    printf(")");
}

void necro_mach_print_store(NecroMachAst* ast, size_t depth)
{
    UNUSED(depth);
    assert(ast->type == NECRO_MACH_STORE);
    printf("store ");
    // switch (ast->store.store_type)
    // {
    // case NECRO_MACH_STORE_TAG:
    //     necro_mach_print_value(ast->store.source_value, NECRO_PRINT_VALUE_TYPE);
    //     printf(" ");
    //     necro_mach_print_value(ast->store.dest_ptr, NECRO_PRINT_VALUE_TYPE);
    //     printf(" (tag)");
    //     return;
    // case NECRO_MACH_STORE_PTR:
        necro_mach_print_value(ast->store.source_value, NECRO_PRINT_VALUE_TYPE);
        printf(" ");
        necro_mach_print_value(ast->store.dest_ptr, NECRO_PRINT_VALUE_TYPE);
        // return;
    // case NECRO_MACH_STORE_SLOT:
    //     necro_mach_print_value(ast->store.source_value, NECRO_PRINT_VALUE_TYPE);
    //     printf(" ");
    //     necro_mach_print_value(ast->store.store_slot.dest_ptr, NECRO_PRINT_VALUE_TYPE);
    //     printf(" (slot %zu)", ast->store.store_slot.dest_slot);
    //     return;
    // }
}

void necro_mach_print_load(NecroMachAst* ast, size_t depth)
{
    UNUSED(depth);
    assert(ast->type == NECRO_MACH_LOAD);
    printf("%%%s = load ", ast->load.dest_value->value.reg_symbol->name->str);
    // switch (ast->load.load_type)
    // {
    // case NECRO_MACH_LOAD_TAG:
    //     necro_mach_print_value(ast->load.source_ptr, NECRO_PRINT_VALUE_TYPE);
    //     printf(" (tag)");
    //     return;
    // case NECRO_MACH_LOAD_PTR:
        necro_mach_print_value(ast->load.source_ptr, NECRO_PRINT_VALUE_TYPE);
    //     return;
    // case NECRO_MACH_LOAD_SLOT:
    //     necro_mach_print_value(ast->load.load_slot.source_ptr, NECRO_PRINT_VALUE_TYPE);
    //     printf(" (slot %zu)", ast->load.load_slot.source_slot);
    //     return;
    // case NECRO_MACH_LOAD_GLOBAL:
    //     necro_mach_print_value(ast->load.source_global, NECRO_PRINT_VALUE_TYPE);
    //     // printf(" ");
    //     // printf(" (global))", ast->load.load_slot.source_slot);
    //     return;
    // }
}

void necro_mach_print_bit_cast(NecroMachAst* ast, size_t depth)
{
    UNUSED(depth);
    assert(ast->type == NECRO_MACH_BIT_CAST);
    printf("%%%s = bitcast ", ast->bit_cast.to_value->value.reg_symbol->name->str);
    necro_mach_print_value(ast->bit_cast.from_value, NECRO_PRINT_VALUE_TYPE);
    printf(" => (");
    necro_mach_type_print_go(ast->bit_cast.to_value->necro_machine_type, false);
    printf(")");
}

void necro_mach_print_zext(NecroMachAst* ast, size_t depth)
{
    UNUSED(depth);
    assert(ast->type == NECRO_MACH_ZEXT);
    printf("%%%s = zext ", ast->bit_cast.to_value->value.reg_symbol->name->str);
    necro_mach_print_value(ast->bit_cast.from_value, NECRO_PRINT_VALUE_TYPE);
    printf(" => (");
    necro_mach_type_print_go(ast->bit_cast.to_value->necro_machine_type, false);
    printf(")");
}

void necro_mach_print_gep(NecroMachAst* ast, size_t depth)
{
    UNUSED(depth);
    assert(ast->type == NECRO_MACH_GEP);
    printf("%%%s = gep", ast->gep.dest_value->value.reg_symbol->name->str);
    for (size_t i = 0; i < ast->gep.num_indices; ++i)
    {
        // printf(" %du32", ast->gep.indices[i]);
        printf(" ");
        necro_mach_print_value(ast->gep.indices[i], NECRO_DONT_PRINT_VALUE_TYPE);
        if (i < ast->gep.num_indices - 1)
            printf(",");
    }
    printf(", ");
    necro_mach_print_value(ast->gep.source_value, NECRO_PRINT_VALUE_TYPE);
}

void necro_mach_print_insert_value(NecroMachAst* ast, size_t depth)
{
    UNUSED(depth);
    assert(ast->type == NECRO_MACH_INSERT_VALUE);
    printf("%%%s = insert ", ast->insert_value.dest_value->value.reg_symbol->name->str);
    necro_mach_print_value(ast->insert_value.aggregate_value, NECRO_PRINT_VALUE_TYPE);
    printf(", ");
    necro_mach_print_value(ast->insert_value.inserted_value, NECRO_PRINT_VALUE_TYPE);
    printf(", %zu", ast->insert_value.index);
}

void necro_mach_print_extract_value(NecroMachAst* ast, size_t depth)
{
    UNUSED(depth);
    assert(ast->type == NECRO_MACH_EXTRACT_VALUE);
    printf("%%%s = extract ", ast->extract_value.dest_value->value.reg_symbol->name->str);
    necro_mach_print_value(ast->extract_value.aggregate_value, NECRO_PRINT_VALUE_TYPE);
    printf(", %zu", ast->extract_value.index);
}

// void necro_mach_print_nalloc(NecroMachAst* ast, size_t depth)
// {
//     UNUSED(depth);
//     assert(ast->type == NECRO_MACH_NALLOC);
//     // if (ast->nalloc.is_constant)
//     //     printf("%%%s = alloc_const (", necro_intern_get_string(ast->nalloc.result_reg->value.reg_symbol->name));
//     // else
//         printf("%%%s = nalloc (", ast->nalloc.result_reg->value.reg_symbol->name->str);
//     necro_mach_type_print_go(ast->nalloc.type_to_alloc, false);
//     printf("), slots: %zu", ast->nalloc.slots_used);
// }

void necro_mach_print_memcpy(NecroMachAst* ast, size_t depth)
{
    assert(ast->type == NECRO_MACH_MEMCPY);
    print_white_space(depth);
    printf("memcpy ");
    necro_mach_print_value(ast->memcpy.dest, NECRO_PRINT_VALUE_TYPE);
    printf(" ");
    necro_mach_print_value(ast->memcpy.source, NECRO_PRINT_VALUE_TYPE);
    printf(" ");
    necro_mach_print_value(ast->memcpy.num_bytes, NECRO_DONT_PRINT_VALUE_TYPE);
}

void necro_mach_print_memset(NecroMachAst* ast, size_t depth)
{
    assert(ast->type == NECRO_MACH_MEMSET);
    print_white_space(depth);
    printf("memset ");
    necro_mach_print_value(ast->memcpy.dest, NECRO_PRINT_VALUE_TYPE);
    printf(" ");
    necro_mach_print_value(ast->memcpy.source, NECRO_PRINT_VALUE_TYPE);
    printf(" ");
    necro_mach_print_value(ast->memcpy.num_bytes, NECRO_PRINT_VALUE_TYPE);
}

// void necro_mach_print_alloca(NecroMachAst* ast, size_t depth)
// {
//     assert(ast->type == NECRO_MACH_ALLOCA);
//     print_white_space(depth);
//     printf("%%%s = alloca (slots: %zu)", ast->alloca.result->value.reg_symbol->name->str, ast->alloca.num_slots);
// }

void necro_mach_print_state_type(NECRO_STATE_TYPE state_type)
{
    switch (state_type)
    {
    case NECRO_STATE_POLY:
        printf("poly ");
        break;
    case NECRO_STATE_CONSTANT:
        printf("constant ");
        break;
    case NECRO_STATE_POINTWISE:
        printf("pointwise");
        break;
    case NECRO_STATE_STATEFUL:
        printf("stateful");
        break;
    }
}

void necro_mach_print_machine_def(NecroMachAst* ast, size_t depth)
{
    assert(ast->type == NECRO_MACH_DEF);
    print_white_space(depth);
    printf("machine %s\n{\n", ast->machine_def.symbol->name->str);
    print_white_space(depth + 4);
    necro_mach_print_state_type(ast->machine_def.state_type);
    printf("\n");
    print_white_space(depth + 4);
    printf("type: ");
    necro_mach_type_print_go(ast->necro_machine_type, true);
    printf("\n");
    // if (ast->machine_def.init_fn != NULL && (ast->machine_def.state_type == NECRO_STATE_STATEFUL || ast->machine_def.state_type == NECRO_STATE_CONSTANT))
    if (ast->machine_def.init_fn != NULL)
    {
        printf("\n");
        necro_mach_print_fn(ast->machine_def.init_fn, depth + 4);
    }
    // if (ast->machine_def.mk_fn != NULL && (ast->machine_def.state_type == NECRO_STATE_STATEFUL || ast->machine_def.state_type == NECRO_STATE_CONSTANT))
    if (ast->machine_def.mk_fn != NULL)
    {
        printf("\n");
        necro_mach_print_fn(ast->machine_def.mk_fn, depth + 4);
    }
    if (ast->machine_def.update_fn != NULL)
    {
        printf("\n");
        necro_mach_print_fn(ast->machine_def.update_fn, depth + 4);
    }

    printf("}\n");
    puts("");
}

void necro_mach_print_binop(NecroMachAst* ast, size_t depth)
{
    assert(ast->type == NECRO_MACH_BINOP);
    print_white_space(depth);
    printf("%%%s = ", ast->binop.result->value.reg_symbol->name->str);
    switch (ast->binop.binop_type)
    {
    case NECRO_PRIMOP_BINOP_IADD:    printf("iadd ");    break;
    case NECRO_PRIMOP_BINOP_ISUB:    printf("isub ");    break;
    case NECRO_PRIMOP_BINOP_IMUL:    printf("imul ");    break;
    case NECRO_PRIMOP_BINOP_IDIV:    printf("idiv ");    break;
    case NECRO_PRIMOP_BINOP_IREM:    printf("irem ");    break;
    case NECRO_PRIMOP_BINOP_UADD:    printf("uadd ");    break;
    case NECRO_PRIMOP_BINOP_USUB:    printf("usub ");    break;
    case NECRO_PRIMOP_BINOP_UMUL:    printf("umul ");    break;
    case NECRO_PRIMOP_BINOP_UDIV:    printf("udiv ");    break;
    case NECRO_PRIMOP_BINOP_UREM:    printf("urem ");    break;
    case NECRO_PRIMOP_BINOP_FADD:    printf("fadd ");    break;
    case NECRO_PRIMOP_BINOP_FSUB:    printf("fsub ");    break;
    case NECRO_PRIMOP_BINOP_FMUL:    printf("fmul ");    break;
    case NECRO_PRIMOP_BINOP_FDIV:    printf("fdiv ");    break;
    case NECRO_PRIMOP_BINOP_FREM:    printf("frem ");    break;
    case NECRO_PRIMOP_BINOP_OR:      printf("or ");      break;
    case NECRO_PRIMOP_BINOP_AND:     printf("and ");     break;
    case NECRO_PRIMOP_BINOP_SHL:     printf("shl ");     break;
    case NECRO_PRIMOP_BINOP_SHR:     printf("shr ");     break;
    case NECRO_PRIMOP_BINOP_SHRA:    printf("shra ");    break;
    case NECRO_PRIMOP_BINOP_XOR:     printf("xor ");     break;
    case NECRO_PRIMOP_BINOP_FAND:    printf("fand ");    break;
    case NECRO_PRIMOP_BINOP_FOR:     printf("for ");     break;
    case NECRO_PRIMOP_BINOP_FXOR:    printf("fxor ");    break;
    case NECRO_PRIMOP_BINOP_FSHL:    printf("fshl ");    break;
    case NECRO_PRIMOP_BINOP_FSHR:    printf("fshr ");    break;
    case NECRO_PRIMOP_BINOP_FSHRA:   printf("fshra ");   break;
    default: assert(false); break;
    }
    necro_mach_print_value(ast->binop.left, NECRO_PRINT_VALUE_TYPE);
    printf(" ");
    necro_mach_print_value(ast->binop.right, NECRO_PRINT_VALUE_TYPE);
}

void necro_mach_print_uop(NecroMachAst* ast, size_t depth)
{
    assert(ast->type == NECRO_MACH_UOP);
    print_white_space(depth);
    printf("%%%s = ", ast->uop.result->value.reg_symbol->name->str);
    switch (ast->uop.uop_type)
    {
    case NECRO_PRIMOP_UOP_IABS:         printf("iabs ");      break;
    case NECRO_PRIMOP_UOP_ISGN:         printf("isgn ");      break;
    case NECRO_PRIMOP_UOP_UABS:         printf("uabs ");      break;
    case NECRO_PRIMOP_UOP_USGN:         printf("usgn ");      break;
    case NECRO_PRIMOP_UOP_ITOI:         printf("itoi ");      break;
    case NECRO_PRIMOP_UOP_ITOU:         printf("itou ");      break;
    case NECRO_PRIMOP_UOP_ITOF:         printf("itof ");      break;
    case NECRO_PRIMOP_UOP_UTOI:         printf("utoi ");      break;
    case NECRO_PRIMOP_UOP_FTRI:         printf("ftri ");      break;
    case NECRO_PRIMOP_UOP_FRNI:         printf("frni ");      break;
    case NECRO_PRIMOP_UOP_FTOF:         printf("ftof ");      break;
    case NECRO_PRIMOP_UOP_FFLR:         printf("fflr ");      break;
    case NECRO_PRIMOP_UOP_FFLR_TO_INT:  printf("fflr2int ");  break;
    case NECRO_PRIMOP_UOP_FCEIL_TO_INT: printf("fceil2int "); break;
    case NECRO_PRIMOP_UOP_FTRNC_TO_INT: printf("ftrnc2int "); break;
    case NECRO_PRIMOP_UOP_FRND_TO_INT:  printf("frnd2int ");  break;
    case NECRO_PRIMOP_UOP_FBREV:        printf("fbrev ");     break;
    case NECRO_PRIMOP_UOP_FTOB:         printf("ftob ");      break;
    case NECRO_PRIMOP_UOP_FFRB:         printf("ffrb ");      break;
    case NECRO_PRIMOP_UOP_NOT:          printf("not ");       break;
    case NECRO_PRIMOP_UOP_FNOT:         printf("fnot ");      break;
    default: assert(false); break;
    }
    necro_mach_print_value(ast->uop.param, NECRO_PRINT_VALUE_TYPE);
}

void necro_mach_print_phi(NecroMachAst* ast, size_t depth)
{
    assert(ast->type == NECRO_MACH_PHI);
    print_white_space(depth);
    printf("%%%s = phi [", ast->phi.result->value.reg_symbol->name->str);
    NecroMachPhiList* values = ast->phi.values;
    while (values != NULL)
    {
        // printf("%s: ", values->data.block->block.symbol->name->str, values->data.value->value.reg_symbol->name->str);
        printf("%s: ", values->data.block->block.symbol->name->str);
        necro_mach_print_value(values->data.value, NECRO_PRINT_VALUE_TYPE);
        if (values->next == NULL)
            printf("]");
        else
            printf(", ");
        values = values->next;
    }
}

void necro_mach_print_cmp(NecroMachAst* ast, size_t depth)
{
    assert(ast->type == NECRO_MACH_CMP);
    print_white_space(depth);
    printf("%%%s = ", ast->cmp.result->value.reg_symbol->name->str);
    switch (ast->cmp.cmp_type)
    {
    case NECRO_PRIMOP_CMP_EQ: printf("eq "); break;
    case NECRO_PRIMOP_CMP_NE: printf("ne "); break;
    case NECRO_PRIMOP_CMP_GT: printf("gt "); break;
    case NECRO_PRIMOP_CMP_GE: printf("ge "); break;
    case NECRO_PRIMOP_CMP_LT: printf("lt "); break;
    case NECRO_PRIMOP_CMP_LE: printf("le "); break;
    default: assert(false && "cmp not handled in case"); break;
    }
    necro_mach_print_value(ast->cmp.left, NECRO_PRINT_VALUE_TYPE);
    printf(" ");
    necro_mach_print_value(ast->cmp.right, NECRO_PRINT_VALUE_TYPE);
}

// void necro_machine_print_select(NecroMachAst* ast, size_t depth)
// {
//     assert(ast->type == NECRO_MACH_SELECT);
//     print_white_space(depth);
//     printf("%%%s = select", ast->select.result->value.reg_symbol->name->str);
//     necro_mach_print_value(ast->select.cmp_value, NECRO_DONT_PRINT_VALUE_TYPE);
//     printf(" ");
//     necro_mach_print_value(ast->select.left, NECRO_PRINT_VALUE_TYPE);
//     printf(" ");
//     necro_mach_print_value(ast->select.right, NECRO_PRINT_VALUE_TYPE);
// }

void necro_mach_print_ast_go(NecroMachAst* ast, size_t depth)
{
    if (ast == NULL)
        return;
    switch (ast->type)
    {
    case NECRO_MACH_VALUE:
        print_white_space(depth);
        necro_mach_print_value(ast, NECRO_PRINT_VALUE_TYPE);
        return;
    case NECRO_MACH_LOAD:
        print_white_space(depth);
        necro_mach_print_load(ast, depth);
        return;
    case NECRO_MACH_STORE:
        print_white_space(depth);
        necro_mach_print_store(ast, depth);
        return;
    case NECRO_MACH_BIT_CAST:
        print_white_space(depth);
        necro_mach_print_bit_cast(ast, depth);
        return;
    case NECRO_MACH_ZEXT:
        print_white_space(depth);
        necro_mach_print_zext(ast, depth);
        return;
    case NECRO_MACH_CALL:
        print_white_space(depth);
        necro_mach_print_call(ast, depth);
        return;
    case NECRO_MACH_CALLI:
        print_white_space(depth);
        necro_mach_print_call_intrinsic(ast, depth);
        return;
    case NECRO_MACH_STRUCT_DEF:
        print_white_space(depth);
        printf("struct ");
        necro_mach_type_print(ast->necro_machine_type);
        return;
    case NECRO_MACH_BLOCK:
        necro_mach_print_block(ast, depth);
        return;
    case NECRO_MACH_FN_DEF:
        print_white_space(depth);
        necro_mach_print_fn(ast, depth);
        return;
    case NECRO_MACH_DEF:
        necro_mach_print_machine_def(ast, depth);
        return;
    case NECRO_MACH_GEP:
        print_white_space(depth);
        necro_mach_print_gep(ast, depth);
        return;
    case NECRO_MACH_EXTRACT_VALUE:
        print_white_space(depth);
        necro_mach_print_extract_value(ast, depth);
        return;
    case NECRO_MACH_INSERT_VALUE:
        print_white_space(depth);
        necro_mach_print_insert_value(ast, depth);
        return;
    case NECRO_MACH_BINOP:
        necro_mach_print_binop(ast, depth);
        return;
    case NECRO_MACH_UOP:
        necro_mach_print_uop(ast, depth);
        return;
    case NECRO_MACH_PHI:
        necro_mach_print_phi(ast, depth);
        return;
    case NECRO_MACH_CMP:
        necro_mach_print_cmp(ast, depth);
        return;
    case NECRO_MACH_MEMCPY:
        necro_mach_print_memcpy(ast, depth);
        return;
    case NECRO_MACH_MEMSET:
        necro_mach_print_memset(ast, depth);
        return;
    // case NECRO_MACH_ALLOCA:
    //     necro_mach_print_alloca(ast, depth);
    //     return;
    // case NECRO_MACH_SELECT:
    //     necro_machine_print_select(ast, depth);
    //     return;
    default:
        assert(false);
        return;
    }
}

void necro_mach_print_ast(NecroMachAst* ast)
{
    necro_mach_print_ast_go(ast, 0);
}

void necro_mach_print_program(NecroMachProgram* program)
{
    printf("///////////////////////////////////////////////////////\n");
    printf("// NecroMachProgram\n");
    printf("///////////////////////////////////////////////////////\n");
    puts("");
    for (size_t i = 0; i < program->structs.length; ++i)
    {
        necro_mach_print_ast_go(program->structs.data[i], 0);
        puts("");
    }
    if (program->structs.length > 0)
        puts("");
    for (size_t i = 0; i < program->globals.length; ++i)
    {
        necro_mach_print_ast_go(program->globals.data[i], 0);
        puts("");
    }
    if (program->globals.length > 0)
        puts("");
    for (size_t i = 0; i < program->functions.length; ++i)
    {
        necro_mach_print_ast_go(program->functions.data[i], 0);
        puts("");
    }
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        necro_mach_print_ast_go(program->machine_defs.data[i], 0);
    }
    necro_mach_print_ast_go(program->necro_init, 0);
    necro_mach_print_ast_go(program->necro_main, 0);
    necro_mach_print_ast_go(program->necro_shutdown, 0);
}

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_MACHINE_BUILD_H
#define NECRO_MACHINE_BUILD_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "core/core.h"
#include "type.h"
#include "utility/arena.h"
#include "machine_type.h"
#include "utility/list.h"
#include "machine.h"

// Utility
NecroVar         necro_gen_var(NecroMachineProgram* program, NecroMachineAST* necro_machine_ast, const char* var_header, NECRO_NAME_UNIQUENESS uniqueness);

// Value
NecroMachineAST* necro_create_machine_value_ast(NecroMachineProgram* program, NecroMachineValue value, NecroMachineType* necro_machine_type);
NecroMachineAST* necro_create_reg(NecroMachineProgram* program, NecroMachineType* necro_machine_type, const char* reg_name_head);
NecroMachineAST* necro_create_global_value(NecroMachineProgram* program, NecroVar global_name, NecroMachineType* necro_machine_type);
NecroMachineAST* necro_create_param_reg(NecroMachineProgram* program, NecroMachineAST* fn_def, size_t param_num);
NecroMachineAST* necro_create_uint1_necro_machine_value(NecroMachineProgram* program, bool uint1_literal);
NecroMachineAST* necro_create_uint8_necro_machine_value(NecroMachineProgram* program, uint8_t uint8_literal);
NecroMachineAST* necro_create_uint16_necro_machine_value(NecroMachineProgram* program, uint16_t uint16_literal);
NecroMachineAST* necro_create_uint32_necro_machine_value(NecroMachineProgram* program, uint32_t uint32_literal);
NecroMachineAST* necro_create_uint64_necro_machine_value(NecroMachineProgram* program, uint64_t uint64_literal);
NecroMachineAST* necro_create_i32_necro_machine_value(NecroMachineProgram* program, int32_t int32_literal);
NecroMachineAST* necro_create_i64_necro_machine_value(NecroMachineProgram* program, int64_t int64_literal);
NecroMachineAST* necro_create_f32_necro_machine_value(NecroMachineProgram* program, float f32_literal);
NecroMachineAST* necro_create_f64_necro_machine_value(NecroMachineProgram* program, double f64_literal);
NecroMachineAST* necro_create_null_necro_machine_value(NecroMachineProgram* program, NecroMachineType* ptr_type);
NecroMachineAST* necro_create_word_uint_value(NecroMachineProgram* program, uint64_t int_literal);
NecroMachineAST* necro_create_word_int_value(NecroMachineProgram* program, int64_t int_literal);
NecroMachineAST* necro_create_word_float_value(NecroMachineProgram* program, double float_literal);

// Blocks
NecroMachineAST* necro_create_machine_block(NecroMachineProgram* program, const char* name, NecroMachineAST* next_block);
void             necro_add_statement_to_block(NecroMachineProgram* program, NecroMachineAST* block, NecroMachineAST* statement);
NecroMachineAST* necro_append_block(NecroMachineProgram* program, NecroMachineAST* fn_def, const char* block_name);
NecroMachineAST* necro_insert_block_before(NecroMachineProgram* program, NecroMachineAST* fn_def, const char* block_name, NecroMachineAST* block_to_precede);
void             necro_move_to_block(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* block);

// Memory
NecroMachineAST* necro_build_nalloc(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineType* type, uint32_t a_slots_used, bool is_constant);
NecroMachineAST* necro_build_gep(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* source_value, uint32_t* a_indices, size_t num_indices, const char* dest_name);
NecroMachineAST* necro_build_bit_cast(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* value, NecroMachineType* to_type);
void             necro_build_memcpy(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* dest, NecroMachineAST* source, NecroMachineAST* num_bytes);

NecroMachineAST* necro_create_machine_struct_def(NecroMachineProgram* program, NecroVar name, NecroMachineType** members, size_t num_members);
NecroMachineAST* necro_create_machine_fn(NecroMachineProgram* program, NecroVar name, NecroMachineAST* call_body, NecroMachineType* necro_machine_type);
NecroMachineAST* necro_create_machine_runtime_fn(NecroMachineProgram* program, NecroVar name, NecroMachineType* necro_machine_type, void* runtime_fn_addr, NECRO_STATE_TYPE state_type);
NecroMachineAST* necro_create_machine_initial_machine_def(NecroMachineProgram* program, NecroVar bind_name, NecroMachineAST* outer, NecroMachineType* value_type, NecroType* necro_value_type);

// Load / Store
void             necro_build_store_into_ptr(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* source_value, NecroMachineAST* dest_ptr);
void             necro_build_store_into_slot(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* source_value, NecroMachineAST* dest_ptr, size_t dest_slot);
NecroMachineAST* necro_build_load_from_ptr(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* source_ptr_ast, const char* dest_name_header);
NecroMachineAST* necro_build_load_from_slot(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* source_ptr_ast, size_t source_slot, const char* dest_name_header);

// Functions
NecroMachineAST* necro_build_call(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* fn_to_call_value, NecroMachineAST** a_parameters, size_t num_parameters, const char* dest_name_header);
NecroMachineAST* necro_build_binop(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* left, NecroMachineAST* right, NECRO_MACHINE_BINOP_TYPE op_type);
NecroMachineAST* necro_build_maybe_cast(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* ast, NecroMachineType* type_to_match);

// Branching
void             necro_build_return(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* return_value);
void             necro_build_break(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* block_to_jump_to);
void             necro_build_cond_break(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* cond, NecroMachineAST* true_block, NecroMachineAST* false_block);
NecroMachineAST* necro_build_cmp(NecroMachineProgram* program, NecroMachineAST* fn_def, NECRO_MACHINE_CMP_TYPE cmp_type, NecroMachineAST* left, NecroMachineAST* right);
NecroMachineAST* necro_build_phi(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineType* type, NecroMachinePhiList* values);
void             necro_add_incoming_to_phi(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* phi, NecroMachineAST* block, NecroMachineAST* value);
void             necro_add_case_to_switch(NecroMachineProgram* program, struct NecroSwitchTerminator* switch_term, NecroMachineAST* block, size_t value);
void             necro_build_unreachable(NecroMachineProgram* program, NecroMachineAST* fn_def);
struct NecroSwitchTerminator* necro_build_switch(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* choice_val, NecroMachineSwitchList* values, NecroMachineAST* else_block);

// Program
void             necro_program_add_struct(NecroMachineProgram* program, NecroMachineAST* struct_ast);
void             necro_program_add_function(NecroMachineProgram* program, NecroMachineAST* function);
void             necro_program_add_machine_def(NecroMachineProgram* program, NecroMachineAST* machine_def);
void             necro_program_add_global(NecroMachineProgram* program, NecroMachineAST* global);

#endif // NECRO_MACHINE_BUILD_H
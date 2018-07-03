/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_NODE_TYPE_H
#define NECRO_NODE_TYPE_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "core/core.h"
#include "type.h"

struct NecroNodeProgram;

typedef enum
{
    NECRO_NODE_TYPE_UINT16,
    NECRO_NODE_TYPE_UINT32,
    NECRO_NODE_TYPE_INT,
    NECRO_NODE_TYPE_DOUBLE,
    NECRO_NODE_TYPE_CHAR,
    NECRO_NODE_TYPE_STRUCT,
    NECRO_NODE_TYPE_FN,
    NECRO_NODE_TYPE_PTR,
} NECRO_NODE_TYPE_TYPE;

typedef struct NecroNodeType
{
    union
    {
        struct NecroNodeStructType
        {
            NecroVar               name;
            struct NecroNodeType** members;
            size_t                 num_members;
        } struct_type;
        struct NecroNodePtrType
        {
            struct NecroNodeType* element_type;
        } ptr_type;
        struct NecroNodeFnType
        {
            NecroVar               name;
            struct NecroNodeType*  return_type;
            struct NecroNodeType** parameters;
            size_t                 num_parameters;
        } fn_type;
    };
    NECRO_NODE_TYPE_TYPE type;
} NecroNodeType;

NecroNodeType* necro_create_node_uint16_type(NecroPagedArena* arena);
NecroNodeType* necro_create_node_uint32_type(NecroPagedArena* arena);
NecroNodeType* necro_create_node_int_type(NecroPagedArena* arena);
NecroNodeType* necro_create_node_double_type(NecroPagedArena* arena);
NecroNodeType* necro_create_node_char_type(NecroPagedArena* arena);
NecroNodeType* necro_create_node_struct_type(NecroPagedArena* arena, NecroVar name, NecroNodeType** a_members, size_t num_members);
NecroNodeType* necro_create_node_fn_type(NecroPagedArena* arena, NecroVar name, NecroNodeType* return_type, NecroNodeType** a_parameters, size_t num_parameters);
NecroNodeType* necro_create_node_ptr_type(NecroPagedArena* arena, NecroNodeType* element_type);
void           necro_type_check(NecroNodeType* type1, NecroNodeType* type2);
void           necro_node_print_node_type(NecroIntern* intern, NecroNodeType* type);
NecroNodeType* necro_core_ast_to_node_type(struct NecroNodeProgram* program, NecroCoreAST_Expression* core_ast);

#endif // NECRO_NODE_TYPE_H
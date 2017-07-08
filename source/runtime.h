/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef RUNTIME_H
#define RUNTIME_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

/*
    Semantic analysis: https://ruslanspivak.com/lsbasi-part13/
    Necronomicon is strict, static, and ref counted, with these exceptions:
        - delay :: const a -> a -> a
          keyword, lazy, uses a weak ptr to second argument
          Induces 1 block sample delay.
          Only takes a constant expression in the first argument, preventing nested delays (and further memory than exactly 1 block).
          This means the language can only ever look back exactly 1 sample in time.
        - poly functions allow dynamic streams.
          These newly created can be destroyed, and thus this necessitates ref counting all nodes.
          GC isn't necessary however.

    // vvv DOES THIS SHIT EVEN WORK!?!?!?!?!?! vvv
    Necronomicon uses lexically scoped time
        - Streams scoped further in can differ in time from streams outer in scope.
        - The callee's time scope (as opposed to the the caller's time scope) determines the callee's time.
        - This differs from arrowized FRP which uses a more dynamically time scoped semantics where the caller determines the time.
        - This allows time streams to be run faster or slower than their parent nodes, and things will just work.

    Different Stream states:
        - Const, Live, Rest, Inhibit, End?
        - Would allow for --> semantics (i.e. actual switching)
        - Pattern type functions such as seq and cycle would be defined in terms of this.

    // Don't this this is possible in the face of dynamic streams actually....
    All variables across the entire runtime are normalized such that they all have exactly unique size_t identifiers
        - Thus there is no "shadowing"
        // - There also is no need for local environments

*/

//=====================================================
// Runtime structs
//=====================================================
typedef struct NecroRuntime NecroRuntime;

typedef enum
{
    //--------------------
    // Value Objects
    NECRO_OBJECT_NULL,
    NECRO_OBJECT_FLOAT,
    NECRO_OBJECT_INT,
    NECRO_OBJECT_CHAR,
    NECRO_OBJECT_BOOL,
    NECRO_OBJECT_AUDIO,

    //--------------------
    // Language Constructs
    NECRO_OBJECT_VAR,
    NECRO_OBJECT_APP,
    NECRO_OBJECT_PAP,
    NECRO_OBJECT_LAMBDA,
    NECRO_OBJECT_PRIM_OP,

    //--------------------
    // Utility Objects
    NECRO_OBJECT_STACK,
    NECRO_OBJECT_FREE

} NECRO_OBJECT_TYPE;

//--------------------
// Language Constructs
typedef struct
{
    uint32_t scope_index;
    uint32_t stack_offset;
} NecroVar;

typedef struct
{
    uint32_t lambda_id;
    uint32_t argument_1_id;
    uint16_t argument_count;
} NecroApp;

typedef struct
{
    uint32_t lambda_id;
    uint32_t argument_1_id;
    uint16_t current_arg_count;
} NecroPap;

typedef struct
{
    uint32_t body_id;
    uint32_t env_id;
    uint16_t arity;
} NecroLambda;

typedef struct
{
    void (*prim_func)(NecroRuntime*);
} NecroPrimOp;

//--------------------
// Utility Objects

// NecroEnv, implemented as a spaghetti stack,
// i.e. a linked list of linked lists, with each node containing:
//     * a pointer to the parent env (stack of stacks),
//     * a pointer to the next node in the env (scoped stack),
//     * a pointer to the value that this node contains
// https://en.wikipedia.org/wiki/Parent_pointer_tree
typedef struct
{
    uint32_t parent_env;
    uint32_t next_env;
    uint32_t value_id;
} NecroEnv;

typedef struct
{
    union
    {
        // Value Objects
        double        float_value;
        int64_t       int_value;
        char          char_value;
        bool          bool_value;
        uint32_t      audio_index;

        // Language Constructs
        NecroVar      var;
        NecroApp      app;
        NecroPap      pap;
        NecroLambda   lambda;
        NecroPrimOp   prim_op;

        // Utility Objects
        NecroEnv      env;
        uint32_t      next_free_index;
    };
    uint32_t          time;
    uint32_t          ref_count;
    NECRO_OBJECT_TYPE type;
} NecroObject;

typedef struct
{
    uint32_t sample_rate;
    uint32_t block_size;
} NecroAudioInfo;

struct NecroRuntime
{
    NecroObject*   objects;
    uint32_t       object_free_list;
    NecroAudioInfo audio_info;
};

NecroRuntime necro_create_runtime(NecroAudioInfo audio_info);

#endif // RUNTIME_H

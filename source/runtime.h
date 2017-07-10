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

// Current value pointers for each construct?
// Gets updated when it's demanded?
/*
    Semantic analysis: https://ruslanspivak.com/lsbasi-part13/
    Necronomicon is strict and ref counted, with these exceptions:
        - delay :: const a -> a -> a
          keyword, lazy, uses a weak ptr to second argument
          Induces 1 block sample delay.
          Only takes a constant expression in the first argument, preventing nested delays (and further memory than exactly 1 block).
          This means the language can only ever look back exactly 1 sample in time.
        - poly functions allow dynamic streams.
          These newly created can be destroyed, and thus this requires ref counting all nodes.
          GC isn't necessary because we are strict and only allow recursion through delay, which uses a weak ptr scheme.

    Necronomicon uses lexically scoped time
        - Streams scoped further in can differ in time from streams outer in scope.
        - The callee's time scope (as opposed to the the caller's time scope) determines the callee's time.
        - This differs from arrowized FRP which uses a more dynamically time scoped semantics where the caller determines the time.
        - This allows time streams to be run faster or slower than their parent nodes, and things will just work.

    Different Stream states:
        - Const, Live, Rest, Inhibit, End?
        - Would allow for --> semantics (i.e. actual switching)
        - Pattern type functions such as seq and cycle would be defined in terms of this.
*/

//=====================================================
// Runtime structs
//=====================================================
typedef struct NecroRuntime NecroRuntime;

typedef struct { uint32_t id; } NecroObjectID;
typedef struct { uint32_t id; } NecroAudioID;

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
    NECRO_OBJECT_LIST_NODE,

    //--------------------
    // Language Constructs
    NECRO_OBJECT_VAR,
    NECRO_OBJECT_APP,
    NECRO_OBJECT_PAP,
    NECRO_OBJECT_LAMBDA,
    NECRO_OBJECT_PRIMOP,

    //--------------------
    // Utility Objects
    NECRO_OBJECT_ENV,
    NECRO_OBJECT_FREE

} NECRO_OBJECT_TYPE;

typedef enum
{
    ADD_I,
    ADD_F,
    ADD_A,

    SUB_I,
    SUB_F,
    SUB_A
} NECRO_PRIM_OP_CODE;

//--------------------
// Value Objects
typedef struct
{
    NecroObjectID value_id;
    NecroObjectID next_id;
} NecroListNode;

//--------------------
// Language Constructs
typedef struct
{
    uint32_t var_symbol;
    uint32_t value_id;
} NecroVar;

typedef struct
{
    NecroObjectID current_value_id;
    NecroObjectID lambda_id;
    NecroObjectID argument_list_id;
    uint16_t      argument_count;
} NecroApp;

typedef struct
{
    NecroObjectID current_value_id;
    NecroObjectID lambda_id;
    NecroObjectID argument_list_id;
    uint16_t      current_arg_count;
} NecroPap;

typedef struct
{
    NecroObjectID current_value_id;
    NecroObjectID body_id;
    NecroObjectID env_id;
    uint16_t      arity;
} NecroLambda;

typedef struct
{
    NecroObjectID current_value_id;
    uint32_t      op; // Should be an enum for this, use a switch to break between different primops
    NecroObjectID env_id;
    uint16_t      arity;
} NecroPrimOp;

//--------------------
// Utility Objects

//--------
// NecroEnv, implemented as a Cactus stack / Parent Pointer tree,
// i.e. a linked list of linked lists, with each node containing:
//     * a pointer to the parent env (stack of envs),
//     * a pointer to the next node in the env,
//     * a var symbol which must be matched against.
//     * a pointer to the value that this node contains
typedef struct
{
    NecroObjectID parent_env;
    NecroObjectID next_env_node;
    uint32_t      var_symbol;
    NecroObjectID value_id;
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
        NecroAudioID  audio_id;
        NecroListNode list_node;

        // Language Constructs
        NecroVar    var;
        NecroApp    app;
        NecroPap    pap;
        NecroLambda lambda;
        NecroPrimOp primop;

        // Utility Objects
        NecroEnv    env;
        uint32_t    next_free_index;
    };
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
    double*        audio;
    uint32_t*      audio_free_list;
    uint32_t       audio_free_list_head;
    NecroAudioInfo audio_info;
};

NecroRuntime  necro_create_runtime(NecroAudioInfo audio_info);
void          necro_destroy_runtime(NecroRuntime* runtime);
NecroObjectID necro_alloc_object(NecroRuntime* runtime);
void          necro_free_object(NecroRuntime* runtime, NecroObjectID object_id);
NecroAudioID  necro_alloc_audio(NecroRuntime* runtime);
void          necro_free_audio(NecroRuntime* runtime, NecroAudioID audio_id);
NecroObjectID necro_create_var(NecroRuntime* runtime, NecroVar var);
NecroObjectID necro_create_app(NecroRuntime* runtime, NecroApp app);
NecroObjectID necro_create_pap(NecroRuntime* runtime, NecroPap pap);
NecroObjectID necro_create_lambda(NecroRuntime* runtime, NecroLambda lambda);
NecroObjectID necro_create_primop(NecroRuntime* runtime, NecroPrimOp primop);
NecroObjectID necro_create_env(NecroRuntime* runtime, NecroEnv env);
NecroObjectID necro_create_float(NecroRuntime* runtime, double value);
NecroObjectID necro_create_int(NecroRuntime* runtime, int64_t value);
NecroObjectID necro_create_char(NecroRuntime* runtime, char value);
NecroObjectID necro_create_bool(NecroRuntime* runtime, bool value);
NecroObjectID necro_create_list_node(NecroRuntime* runtime, NecroListNode list_node);

void          necro_test_runtime();

#endif // RUNTIME_H

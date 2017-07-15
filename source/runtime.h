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
    Idea #1
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

    // Multiple fby's breaks this!?
    Idea #2
    Lucid style evaluation model :
        * An immutable Network where each node can process from any time N to time N + 1
        * Evaluation is fully lazy and allows full recursion, as long as a delay is introduced.
        * Each value has a place (variable) and time (time slice)).
        * This means you need envs for variable lookups, but you also need envs for time lookups!
        * However instead of their "Retirement Age" scheme, we use "Time Streams".

        * Each node caches a different value for each separate "Time stream" (i.e. Pattern slot, or synth),
          as well as caching the nodes value at time 0

        * This means that there is at most on cached value for each time stream (constant space)
        * Separate time streams can no longer share work, however they also always calculate one time slice value each tick (constant time)
        * This also supports LexicalTime scoping, since you simply look up the node's value for the non-local time stream up in the next scope in your env

        * This all works because we only provide the programmer with combinators that can at most reach one time slice into the past,
          such as fby (delays one sample), and [] sequences (each stream is stepped forward one time slice at a time)
        * Rewinding and other nonsense is strictly prohibited

        * This scheme works better for Necronomicon because we'd rather pay a little bit more overhead
          overall to maintain a clean constant time/space system rather than rely on complex
          GC system that could at any point pause the system for a long period of time.
        * The other upshot is the language gets to have correct lazy semantics

    // Idea #3
    All in on Lexical time scoping and pattern sequences:
        - No more "fby"
        - In lieu we have pattern sequences: they keep going until the sequence "yields" (as opposed to fby, which immediately switches from seq1 to seq2)
        - This simplifies the language and the semantics, since there's only 1 mechanism to introduce time asynchronicity
        - Each node cache's it's last value only
        - Only need ref counting for when streams get torn down
        - Streams get replicated when part of a pattern sequence or when spawned by poly

    Lexical time scoping
        - By default Necronomicon uses local time semantics
        - i.e. when you introduce a new time stream via fby or pattern sequences [],
          the values proceed, such that: x fby y == { x0, y0, y1, y2, ... };
        - Necronomicon optionally allows the user to use switch to "Lexical Time scoping" via the unary ~ operator
        - Thus, x fby ~y == { x0, y1, y2, y3, y4, ... }
        - More precisely it means reference the time stream y that already exists in this scope instead of
          creating a fresh time stream of y.
        - If there is no higher scoped version of that sequence (i.e. the sequence is defined at the same scope),
          then this simply translates into a noop.

    Different Stream states:
        - Const, Live, Rest, Inhibit, End?
        - Would allow for --> semantics (i.e. actual switching)
        - Pattern type functions such as seq and cycle would be defined in terms of this.

    Lists are simply chained "pattern" sequences (similar to fbys)?
    - Semantics are a little different than fbys:
    - Each sequence continues until it "yields"
    - Then sequence moves on to next stream
    myCoolLoop = 0 fby 100 fby 1 + myCoolLoop  == { 0, 100, 1 ... }
    myCoolLoop = 0 fby 100 fby 1 + ~myCoolLoop ==
*/

//=====================================================
// Runtime structs
//=====================================================

// Runtime representation is something akin to lambda calculus meets lisp, with graph update (instead of graph reduction) semantics

typedef struct NecroRuntime NecroRuntime;

typedef struct { uint32_t id; } NecroObjectID;
typedef struct { uint32_t id; } NecroAudioID;

typedef enum
{
    NECRO_CONSTANT,
    NECRO_LIVE,
    NECRO_INHIBIT,
    NECRO_END
} NECRO_SIGNAL_STATE;

// TODO: Tuple type?
typedef enum
{
    //--------------------
    // Value Objects
    NECRO_OBJECT_NULL,
    NECRO_OBJECT_INHIBIT,
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
    NECRO_OBJECT_PRIMOP,
    NECRO_OBJECT_SEQUENCE,

    //--------------------
    // Utility Objects
    NECRO_OBJECT_ENV,
    NECRO_OBJECT_LIST_NODE,
    NECRO_OBJECT_FREE

} NECRO_OBJECT_TYPE;

typedef enum
{
    NECRO_PRIM_ADD_I,
    NECRO_PRIM_ADD_F,
    NECRO_PRIM_ADD_A,
    NECRO_PRIM_SUB_I,
    NECRO_PRIM_SUB_F,
    NECRO_PRIM_SUB_A
} NECRO_PRIM_OP_CODE;

// In Necronomicon, Lists are temporal constructs
// However, they can also be used internall for list structures
//--------------------
// Language Constructs
typedef struct
{
    uint32_t      var_symbol;
    NecroObjectID cached_env_node_id;
} NecroVar;

typedef struct
{
    NecroObjectID current_value_id;
    NecroObjectID lambda_id;
    NecroObjectID argument_list_id;
    uint32_t      argument_count;
} NecroApp;

typedef struct
{
    NecroObjectID lambda_id;
    NecroObjectID argument_list_id;
    uint32_t      current_arg_count;
} NecroPap;

typedef struct
{
    NecroObjectID body_id;
    NecroObjectID env_id;
    NecroObjectID where_list_id;
    uint32_t      arity;
} NecroLambda;

typedef struct
{
    uint32_t      op;
} NecroPrimOp;

typedef struct
{
    NecroObjectID head;
    NecroObjectID current;
    uint32_t      count;
} NecroSequence;

//--------------------
// Utility Objects

//--------
// NecroEnv, implemented as a Cactus stack / Parent Pointer tree,
// i.e. a linked list of nodes, with keys and values, with shadowing
//     * a pointer to the next env node,
//     * a key which must be matched against.
//     * a pointer to the value that this node contains
typedef struct
{
    NecroObjectID next_env_id;
    uint32_t      key;
    NecroObjectID value_id;
} NecroEnv;

typedef struct
{
    NecroObjectID value_id;
    NecroObjectID next_id;
} NecroListNode;

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
        NecroSequence sequence;

        // Language Constructs
        NecroVar    var;
        NecroApp    app;
        NecroPap    pap;
        NecroLambda lambda;
        NecroPrimOp primop;

        // Utility Objects
        NecroEnv      env;
        NecroListNode list_node;
        uint32_t      next_free_index;
    };
    uint32_t           ref_count;
    NECRO_OBJECT_TYPE  type;
    NECRO_SIGNAL_STATE signal_state;
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
NecroObjectID necro_eval(NecroRuntime* runtime, NecroObjectID env, NecroObjectID object);
void          necro_print_object(NecroRuntime* runtime, NecroObjectID object);

void          necro_test_runtime();
void          necro_test_eval();

#endif // RUNTIME_H

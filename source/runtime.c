/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "runtime.h"

// Trying out a 4 : 1 ratio of language objects per audio block for now
#define NECRO_INITIAL_NUM_OBJECT_SLABS  16384
#define NECRO_INITIAL_NUM_AUDIO_BUFFERS 4096

// ObjectIDs
#define NULL_NECRO_OBJECT_ID    (NecroObjectID) { 0 }
#define YIELD_NECRO_OBJECT_ID (NecroObjectID) { 1 }

// Raw IDs
#define NULL_ID       0
#define YIELD_ID      1
#define MAX_NECRO_ID -1

//=====================================================
// Initialization And Cleanup
//=====================================================
NecroRuntime necro_create_runtime(NecroAudioInfo audio_info)
{
    // Allocate all runtime memory as a single contiguous chunk
    // NOTE: Making this contiguous makes reallocation when running out of memory more complicated,
    // but might increase cache performance.
    size_t size_of_objects         = NECRO_INITIAL_NUM_OBJECT_SLABS  * sizeof(NecroObject);
    size_t size_of_audio_free_list = NECRO_INITIAL_NUM_AUDIO_BUFFERS * sizeof(uint32_t);
    size_t size_of_audio           = NECRO_INITIAL_NUM_AUDIO_BUFFERS * (audio_info.block_size * sizeof(double));
    char*  runtime_memory          = malloc(size_of_objects + size_of_audio + size_of_audio_free_list);
    if (runtime_memory == NULL)
    {
        fprintf(stderr, "Malloc returned NULL when allocating memory for runtime in necro_create_runtime()\n");
        exit(1);
    }

    // Initialize NecroObject slabs
    NecroObject* objects = (NecroObject*)runtime_memory;
    objects[0].type      = NECRO_OBJECT_NULL;
    objects[1].type      = NECRO_OBJECT_YIELD;
    for (uint32_t i = 2; i < NECRO_INITIAL_NUM_OBJECT_SLABS; ++i)
    {
        objects[i].next_free_index = i - 1;
        objects[i].ref_count       = 0;
        objects[i].type            = NECRO_OBJECT_FREE;
    }

    // Initialize Audio and AudioFreeList
    uint32_t* audio_free_list = (uint32_t*)(runtime_memory + size_of_objects);
    double*   audio           = (double*)  (runtime_memory + size_of_objects + size_of_audio_free_list);
    memset(audio, 0, size_of_audio);
    for (size_t i = 1; i < NECRO_INITIAL_NUM_AUDIO_BUFFERS; ++i)
    {
        audio_free_list[i] = i - 1 ;
    }

    // return NecroRuntime struct
    return (NecroRuntime)
    {
        objects,
        NECRO_INITIAL_NUM_OBJECT_SLABS - 1,
        audio,
        audio_free_list,
        NECRO_INITIAL_NUM_AUDIO_BUFFERS - 1
    };
}

void necro_destroy_runtime(NecroRuntime* runtime)
{
    if (runtime->objects != NULL)
    {
        free(runtime->objects);
        runtime->objects         = NULL;
        runtime->audio           = NULL;
        runtime->audio_free_list = NULL;
    }
    runtime->object_free_list     = 0;
    runtime->audio_free_list_head = 0;
}

//=====================================================
// Allocation and Freeing
//=====================================================
NecroAudioID necro_alloc_audio(NecroRuntime* runtime)
{
    uint32_t current_free = runtime->audio_free_list[runtime->audio_free_list_head];
    if (current_free == 0)
    {
        fprintf(stderr, "Runtime error: Runtime memory exhausted while allocating audio block!\n");
        exit(1);
    }
    runtime->audio_free_list_head--;
    return (NecroAudioID) { current_free };
}

void necro_free_audio(NecroRuntime* runtime, NecroAudioID audio_id)
{
    runtime->audio_free_list_head++;
    runtime->audio_free_list[runtime->audio_free_list_head] = audio_id.id;
}

NecroObjectID necro_alloc_object(NecroRuntime* runtime)
{
    uint32_t current_free = runtime->object_free_list;
    if (current_free == 0)
    {
        fprintf(stderr, "Runtime error: Runtime memory exhausted while allocating NecroObject!\n");
        exit(1);
    }
    uint32_t next_free                       = runtime->objects[current_free].next_free_index;
    runtime->object_free_list                = next_free;
    runtime->objects[current_free].type      = NECRO_OBJECT_NULL;
    runtime->objects[current_free].ref_count = 0;
    return (NecroObjectID) { current_free };
}

void necro_free_object(NecroRuntime* runtime, NecroObjectID object_id)
{
    runtime->objects[object_id.id].type            = NECRO_OBJECT_FREE;
    runtime->objects[object_id.id].next_free_index = runtime->object_free_list;
    runtime->object_free_list                      = object_id.id;
}

static inline NecroObject* necro_get_object(NecroRuntime* runtime, NecroObjectID object_id)
{
    // Higher bounds check?
    if (object_id.id == 0)
        return NULL;
    return &runtime->objects[object_id.id];
}

NecroObjectID necro_create_var(NecroRuntime* runtime, NecroVar var)
{
    NecroObjectID id             = necro_alloc_object(runtime);
    runtime->objects[id.id].type = NECRO_OBJECT_VAR;
    runtime->objects[id.id].var  = var;
    return id;
}

NecroObjectID necro_create_app(NecroRuntime* runtime, NecroApp app)
{
    NecroObjectID id             = necro_alloc_object(runtime);
    runtime->objects[id.id].type = NECRO_OBJECT_APP;
    runtime->objects[id.id].app  = app;
    return id;
}

NecroObjectID necro_create_pap(NecroRuntime* runtime, NecroPap pap)
{
    NecroObjectID id             = necro_alloc_object(runtime);
    runtime->objects[id.id].type = NECRO_OBJECT_PAP;
    runtime->objects[id.id].pap  = pap;
    return id;
}

NecroObjectID necro_create_lambda(NecroRuntime* runtime, NecroLambda lambda)
{
    NecroObjectID id               = necro_alloc_object(runtime);
    runtime->objects[id.id].type   = NECRO_OBJECT_LAMBDA;
    runtime->objects[id.id].lambda = lambda;
    return id;
}

NecroObjectID necro_create_primop(NecroRuntime* runtime, NecroPrimOp primop)
{
    NecroObjectID id               = necro_alloc_object(runtime);
    runtime->objects[id.id].type   = NECRO_OBJECT_PRIMOP;
    runtime->objects[id.id].primop = primop;
    return id;
}

NecroObjectID necro_create_env(NecroRuntime* runtime, NecroEnv env)
{
    NecroObjectID id             = necro_alloc_object(runtime);
    runtime->objects[id.id].type = NECRO_OBJECT_ENV;
    runtime->objects[id.id].env  = env;
    return id;
}

NecroObjectID necro_create_float(NecroRuntime* runtime, double value)
{
    NecroObjectID id                    = necro_alloc_object(runtime);
    runtime->objects[id.id].type        = NECRO_OBJECT_FLOAT;
    runtime->objects[id.id].float_value = value;
    return id;
}

NecroObjectID necro_create_int(NecroRuntime* runtime, int64_t value)
{
    NecroObjectID id                  = necro_alloc_object(runtime);
    runtime->objects[id.id].type      = NECRO_OBJECT_INT;
    runtime->objects[id.id].int_value = value;
    return id;
}

NecroObjectID necro_create_char(NecroRuntime* runtime, char value)
{
    NecroObjectID id                   = necro_alloc_object(runtime);
    runtime->objects[id.id].type       = NECRO_OBJECT_CHAR;
    runtime->objects[id.id].char_value = value;
    return id;
}

NecroObjectID necro_create_bool(NecroRuntime* runtime, bool value)
{
    NecroObjectID id                   = necro_alloc_object(runtime);
    runtime->objects[id.id].type       = NECRO_OBJECT_BOOL;
    runtime->objects[id.id].bool_value = value;
    return id;
}

NecroObjectID necro_create_list_node(NecroRuntime* runtime, NecroListNode list_node)
{
    NecroObjectID id                  = necro_alloc_object(runtime);
    runtime->objects[id.id].type      = NECRO_OBJECT_LIST_NODE;
    runtime->objects[id.id].list_node = list_node;
    return id;
}

NecroObjectID necro_create_sequence(NecroRuntime* runtime, NecroSequence sequence)
{
    NecroObjectID id                 = necro_alloc_object(runtime);
    runtime->objects[id.id].type     = NECRO_OBJECT_SEQUENCE;
    runtime->objects[id.id].sequence = sequence;
    return id;
}

//=====================================================
// Env
//=====================================================
// Returns NecroObjectID of the first env_node who's key matches the provided key
// If no match is found NULL is returned instead.
// To be clear this returns the ENV NODE's id, not the actual Variable's ID
// It is the responsibility of the caller to dereference the env node to get the CURRENT value at that env_node
NecroObjectID necro_env_lookup(NecroRuntime* runtime, NecroObjectID env, uint32_t key)
{
    while (env.id != 0)
    {
        if (runtime->objects[env.id].env.key == key)
            return env;
        else
            env = runtime->objects[env.id].env.next_env_id;
    }
    return NULL_NECRO_OBJECT_ID;
}

//=====================================================
// Evaluation
//=====================================================
// TODO: Need to figure out where and when to update reference counts!
// necro_eval_app_lambda:
//  * Calling convention assumes that the first N entires in the env correspond to the N arguments supplied to the function, and the next X entries correspond to the W definitions in the lambda's where statement
//  * Or to put it more succinctly, the first N + X nodes of the env form the local stack frame of a cactus stack.
//  * Assumes Env, App, and Lambda are already evaluated
static inline NecroObjectID necro_eval_app_lambda(NecroRuntime* runtime, NecroObjectID env, NecroObjectID app, NecroObjectID lambda)
{
    // Assert Preconditions
    assert(env.id     != 0);
    assert(app.id     != 0);
    assert(lambda.id  != 0);
    assert(runtime->objects[env.id].type    == NECRO_OBJECT_ENV);
    assert(runtime->objects[app.id].type    == NECRO_OBJECT_APP);
    assert(runtime->objects[lambda.id].type == NECRO_OBJECT_LAMBDA);
    // If the Lambda's arity and the App's argument count match
    // Or perhaps we just expect to arrive in this form and as a pap otherwise?
    if (runtime->objects[lambda.id].lambda.arity == runtime->objects[app.id].app.argument_count)
    {
        // Iterate through arg list and first N entries of the lambda's env
        // Strictly evaluate each argument node and store the latest value in the matching env node
        NecroObjectID current_arg_node = runtime->objects[app.id].app.argument_list_id;
        NecroObjectID current_env_node = runtime->objects[lambda.id].lambda.env_id;
        while (current_arg_node.id != 0)
        {
            runtime->objects[current_env_node.id].env.value_id = necro_eval(runtime, env, runtime->objects[current_arg_node.id].list_node.value_id);
            current_arg_node = runtime->objects[current_arg_node.id].list_node.next_id;
            current_env_node = runtime->objects[current_env_node.id].env.next_env_id;
        }
        // Iterate through arg list and next N entries of the lambda's env
        // Strictly evaluate each where node and store the latest value in the matching env node
        NecroObjectID lambda_env_id      = runtime->objects[lambda.id].lambda.env_id;
        NecroObjectID current_where_node = runtime->objects[lambda.id].lambda.where_list_id;
        while (current_where_node.id != 0)
        {
            runtime->objects[current_env_node.id].env.value_id = necro_eval(runtime, lambda_env_id, runtime->objects[current_where_node.id].list_node.value_id);
            current_where_node = runtime->objects[current_where_node.id].list_node.next_id;
            current_env_node   = runtime->objects[current_env_node.id].env.next_env_id;
        }
        // Evaluate body of lambda and assign it to the application's current_value, then return the current value's id
        runtime->objects[app.id].app.current_value_id = necro_eval(runtime, runtime->objects[lambda.id].lambda.env_id, runtime->objects[lambda.id].lambda.body_id);
        return runtime->objects[app.id].app.current_value_id;
    }
    // Else, if there are less arguments, create a Partial Application
    else
    {
        // NecroObjectID lambda_id;
        // NecroObjectID argument_list_id;
        // uint32_t      current_arg_count;
        // NecroObjectID pap = necro_createpap(runtime, {I/)
        return NULL_NECRO_OBJECT_ID;
    }
}

// necro_eval_app_prim_op:
//  * Calling convention assumes that the first N entires in the env correspond to the N arguments supplied to the function!
//  * Assumes Env, App, and PrimOp are already evaluated
static inline NecroObjectID necro_eval_app_prim_op(NecroRuntime* runtime, NecroObjectID env, NecroObjectID app, NecroObjectID prim_op)
{
    // Assert Preconditions
    assert(env.id     != 0);
    assert(app.id     != 0);
    assert(prim_op.id != 0);
    assert(runtime->objects[env.id].type     == NECRO_OBJECT_ENV);
    assert(runtime->objects[app.id].type     == NECRO_OBJECT_APP);
    assert(runtime->objects[prim_op.id].type == NECRO_OBJECT_PRIMOP);
    switch (runtime->objects[prim_op.id].primop.op)
    {
    case NECRO_PRIM_ADD_I:
        {
            // ObjectIDs
            NecroObjectID arg_list_node_1 = runtime->objects[app.id].app.argument_list_id;
            NecroObjectID arg_list_node_2 = runtime->objects[arg_list_node_1.id].list_node.next_id;
            NecroObjectID arg_1           = necro_eval(runtime, env, runtime->objects[arg_list_node_1.id].list_node.value_id);
            NecroObjectID arg_2           = necro_eval(runtime, env, runtime->objects[arg_list_node_2.id].list_node.value_id);
            NecroObjectID current_value   = runtime->objects[app.id].app.current_value_id;
            // Assertions
            assert(arg_list_node_1.id != 0);
            assert(arg_list_node_2.id != 0);
            assert(arg_1.id           != 0);
            assert(arg_2.id           != 0);
            assert(current_value.id   != 0);
            assert(runtime->objects[arg_1.id].type         == NECRO_OBJECT_INT);
            assert(runtime->objects[arg_2.id].type         == NECRO_OBJECT_INT);
            assert(runtime->objects[current_value.id].type == NECRO_OBJECT_INT);
            // Calculate value and return
            runtime->objects[current_value.id].int_value = runtime->objects[arg_1.id].int_value + runtime->objects[arg_2.id].int_value;
            return current_value;
        }
    default:
        return NULL_NECRO_OBJECT_ID;
    }
}

// TODO: Replace recursive function calls with explicit Stack?

// TODO: Semantics for checking for cached values and returning that if it's already been computed!
// necro_eval:
//     * All evaluation starts here, uses AST Walking
//     * Will evaluate the provided object at the given id and return the resultant value's id
//     * If this is a constant (Int, Bool, Lambda, etc) that essentially means simply returning the same ID
//     * Function application will evaluate the result of the application, cache the result's id in the app's current value, and return the result's id.
//     * Variables will return the value id which they point. If the value id is not cached it will perform an env_lookup in the provided envelope, then cache the result.
//     * Caching is useful in Necronomicon because AST nodes are long living entities that will be evaluated many times over.
NecroObjectID necro_eval(NecroRuntime* runtime, NecroObjectID env, NecroObjectID object)
{
    // printf("necro_eval");
    switch (runtime->objects[object.id].type)
    {
    case NECRO_OBJECT_APP:
    {
        NecroObjectID lambda_id = necro_eval(runtime, env, runtime->objects[object.id].app.lambda_id);
        if (runtime->objects[lambda_id.id].type == NECRO_OBJECT_PRIMOP)
            return necro_eval_app_prim_op(runtime, env, object, lambda_id);
        else if (runtime->objects[lambda_id.id].type == NECRO_OBJECT_LAMBDA)
            return necro_eval_app_lambda(runtime, env, object, lambda_id);
        // TODO: FINISH!
        // else if (runtime->objects[lambda_id].type == NECRO_OBJECT_PAP)
        fprintf(stderr, "Object cannot be applied as a function: %d\n", runtime->objects[lambda_id.id].type);
        assert(false);
        return NULL_NECRO_OBJECT_ID;
    }
    case NECRO_OBJECT_VAR:
        if (runtime->objects[object.id].var.cached_env_node_id.id == 0)
            runtime->objects[object.id].var.cached_env_node_id = necro_env_lookup(runtime, env, runtime->objects[object.id].var.var_symbol);
        return runtime->objects[runtime->objects[object.id].var.cached_env_node_id.id].env.value_id;
    case NECRO_OBJECT_SEQUENCE:
        if (runtime->objects[object.id].sequence.current.id == -1)
        {
            runtime->objects[object.id].sequence.current = runtime->objects[object.id].sequence.head;
        }
        else if (runtime->objects[object.id].sequence.current.id != 0)
        {
            runtime->objects[object.id].sequence.current = runtime->objects[runtime->objects[object.id].sequence.current.id].list_node.next_id;
        }
        return runtime->objects[object.id].sequence.current;
    case NECRO_OBJECT_FLOAT:
    case NECRO_OBJECT_INT:
    case NECRO_OBJECT_CHAR:
    case NECRO_OBJECT_BOOL:
    case NECRO_OBJECT_AUDIO:
    case NECRO_OBJECT_LAMBDA:
    case NECRO_OBJECT_PAP:
    case NECRO_OBJECT_PRIMOP:
        return object;
    default:
        fprintf(stderr, "Unsupported type in necro_eval: %d, found at id: %d\n", runtime->objects[object.id].type, object.id);
        assert(false);
        return NULL_NECRO_OBJECT_ID;
    }
}

static inline void necro_print_spaces(uint64_t depth)
{
    for (size_t i = 0; i < depth; ++i)
        printf(" ");
}

void necro_print_object_go(NecroRuntime* runtime, NecroObjectID object, uint64_t depth)
{
    // necro_print_space(depth);
    // printf("(");
    // switch (runtime->objects[object.id].type)
    // {
    // case NECRO_OBJECT_APP:
    //     printf("App ")
    // case NECRO_OBJECT_VAR:
    // case NECRO_OBJECT_LAMBDA:
    // case NECRO_OBJECT_PRIMOP:
    // case NECRO_OBJECT_PAP:
    // case NECRO_OBJECT_FLOAT:
    // case NECRO_OBJECT_INT:
    // case NECRO_OBJECT_CHAR:
    // case NECRO_OBJECT_BOOL:
    // case NECRO_OBJECT_AUDIO:
    // default: break;
    // }
    // necro_print_space(depth);
    // printf("(");
}

void necro_print_object(NecroRuntime* runtime, NecroObjectID object)
{
    necro_print_object_go(runtime, object, 0);
}

//=====================================================
// Testing
//=====================================================
void necro_run_test(bool condition, const char* print_string)
{
    if (condition)
        printf("%s passed\n", print_string);
    else
        printf("%s failed\n", print_string);
}

void necro_test_runtime()
{
    puts("\n");
    puts("--------------------------------");
    puts("-- Testing NecroRuntime");
    puts("--------------------------------\n");

    // Initialize Runtime
    NecroRuntime runtime = necro_create_runtime((NecroAudioInfo) { 44100, 512 });
    necro_run_test(runtime.audio != NULL && runtime.objects != NULL, "NecroRuntime alloc test:        ");

    // Alloc Object
    NecroObjectID object_id = necro_alloc_object(&runtime);
    necro_run_test(runtime.objects[object_id.id].type == NECRO_OBJECT_NULL && runtime.object_free_list != object_id.id && runtime.objects[object_id.id].ref_count == 0, "NecroRuntime alloc object test: ");

    // Free Object
    necro_free_object(&runtime, object_id);
    necro_run_test(runtime.objects[object_id.id].type == NECRO_OBJECT_FREE && runtime.object_free_list == object_id.id, "NecroRuntime free object test:  ");

    // Alloc Audio
    NecroAudioID audio_id = necro_alloc_audio(&runtime);
    necro_run_test(runtime.audio_free_list[runtime.audio_free_list_head] != audio_id.id && audio_id.id != 0, "NecroRuntime alloc audio test:  ");

    // Free Audio
    necro_free_audio(&runtime, audio_id);
    necro_run_test(runtime.audio_free_list[runtime.audio_free_list_head] == audio_id.id, "NecroRuntime free object test:  ");

    // Create Var
    NecroObjectID var        = necro_create_var(&runtime, (NecroVar) { 0, 1 });
    NecroObject*  var_object = necro_get_object(&runtime, var);
    necro_run_test(var_object != NULL && var_object->type == NECRO_OBJECT_VAR && var_object->var.var_symbol == 0 && var_object->var.cached_env_node_id.id == 1, "NecroRuntime create var test:   ");

    // Create App
    NecroObjectID app        = necro_create_app(&runtime, (NecroApp) { 1, 2, 3, 4 });
    NecroObject*  app_object = necro_get_object(&runtime, app);
    necro_run_test(app_object != NULL && app_object->type == NECRO_OBJECT_APP && app_object->app.current_value_id.id == 1 && app_object->app.lambda_id.id == 2 && app_object->app.argument_list_id.id == 3 && app_object->app.argument_count == 4, "NecroRuntime create app test:   ");

    // Create Papp
    NecroObjectID pap        = necro_create_pap(&runtime, (NecroPap) { 3, 4, 5 });
    NecroObject*  pap_object = necro_get_object(&runtime, pap);
    necro_run_test(pap_object != NULL && pap_object->type == NECRO_OBJECT_PAP && pap_object->pap.lambda_id.id == 3 && pap_object->pap.argument_list_id.id == 4 && pap_object->pap.current_arg_count == 5, "NecroRuntime create pap test:   ");

    // Create Lambda
    NecroObjectID lam        = necro_create_lambda(&runtime, (NecroLambda) { 4, 5, 6, 7 });
    NecroObject*  lam_object = necro_get_object(&runtime, lam);
    necro_run_test(lam_object != NULL && lam_object->type == NECRO_OBJECT_LAMBDA && lam_object->lambda.body_id.id == 4 && lam_object->lambda.env_id.id == 5 && lam_object->lambda.where_list_id.id == 6 && lam_object->lambda.arity == 7, "NecroRuntime create lam test:   ");

    // Create PrimOp
    NecroObjectID pri        = necro_create_primop(&runtime, (NecroPrimOp) { 5 });
    NecroObject*  pri_object = necro_get_object(&runtime, pri);
    necro_run_test(pri_object != NULL && pri_object->type == NECRO_OBJECT_PRIMOP && pri_object->primop.op == 5, "NecroRuntime create primop test:");

    // Create Env
    NecroObjectID env        = necro_create_env(&runtime, (NecroEnv) { 5, 6, 7 });
    NecroObject*  env_object = necro_get_object(&runtime, env);
    necro_run_test(env_object != NULL && env_object->type == NECRO_OBJECT_ENV && env_object->env.next_env_id.id == 5 && env_object->env.key == 6 && env_object->env.value_id.id == 7, "NecroRuntime create env test:   ");

    // Create Float
    NecroObjectID f            = necro_create_float(&runtime, 100.5);
    NecroObject*  float_object = necro_get_object(&runtime, f);
    necro_run_test(float_object != NULL && float_object->type == NECRO_OBJECT_FLOAT && float_object->float_value == 100.5, "NecroRuntime create float test: ");

    // Create Int
    NecroObjectID i          = necro_create_int(&runtime, 666);
    NecroObject*  int_object = necro_get_object(&runtime, i);
    necro_run_test(int_object != NULL && int_object->type == NECRO_OBJECT_INT && int_object->int_value == 666, "NecroRuntime create int test:   ");

    // Create Char
    NecroObjectID c           = necro_create_char(&runtime, 'N');
    NecroObject*  char_object = necro_get_object(&runtime, c);
    necro_run_test(char_object != NULL && char_object->type == NECRO_OBJECT_CHAR && char_object->char_value == 'N', "NecroRuntime create char test:  ");

    // Create Bool
    NecroObjectID b           = necro_create_bool(&runtime, true);
    NecroObject*  bool_object = necro_get_object(&runtime, b);
    necro_run_test(bool_object != NULL && bool_object->type == NECRO_OBJECT_BOOL && bool_object->bool_value == true, "NecroRuntime create bool test:  ");

    // Create ListNode
    NecroObjectID l           = necro_create_list_node(&runtime, (NecroListNode) { 7, 8 });
    NecroObject*  list_object = necro_get_object(&runtime, l);
    necro_run_test(list_object != NULL && list_object->type == NECRO_OBJECT_LIST_NODE && list_object->list_node.value_id.id == 7 && list_object->list_node.next_id.id == 8, "NecroRuntime create list test:  ");

    // Create Sequence
    NecroObjectID s   = necro_create_sequence(&runtime, (NecroSequence) { .head = l, .current = NULL_NECRO_OBJECT_ID, .count = 0});
    NecroObject*  seq = necro_get_object(&runtime, s);
    necro_run_test(seq != NULL && seq->type == NECRO_OBJECT_SEQUENCE && seq->sequence.head.id == l.id && seq->sequence.current.id == 0 && seq->sequence.count == 0, "NecroRuntime create seq test:   ");

    // Destroy Runtime
    necro_destroy_runtime(&runtime);
    necro_run_test(runtime.audio == NULL && runtime.objects == NULL && runtime.audio_free_list == NULL, "NecroRuntime destroy test:      ");

    necro_test_eval();
}

void necro_test_expr_eval(NecroRuntime* runtime, const char* print_string, NecroObjectID result, NECRO_OBJECT_TYPE type, int64_t result_to_match)
{
    if (result.id != 0)
        printf("%s NULL test:   passed\n", print_string);
    else
        printf("%s NULL test:   failed!\n", print_string);
    if (runtime->objects[result.id].type == type)
        printf("%s Type test:   passed\n", print_string);
    else
        printf("%s Type test:   failed!\n", print_string);
    if (runtime->objects[result.id].int_value == result_to_match)
        printf("%s Result test: passed\n", print_string);
    else
        printf("%s Result test: failed!\n", print_string);
}

void necro_test_eval()
{
    puts("\n");
    puts("--------------------------------");
    puts("-- Testing NecroRuntime eval");
    puts("--------------------------------");

    puts("\n------\neval1:\n");
    // Initialize Runtime
    NecroRuntime runtime = necro_create_runtime((NecroAudioInfo) { 44100, 512 });

    {
        // Eval 1:
        // 2 + 3
        // translates to:
        // (App 0 (+) (2 3 ()) 2)

        // TODO: Create debug printing for runtime asts
        NecroObjectID constant_2    = necro_create_int(&runtime, 2);
        NecroObjectID constant_3    = necro_create_int(&runtime, 3);
        NecroObjectID plus          = necro_create_primop(&runtime, (NecroPrimOp) { NECRO_PRIM_ADD_I });
        NecroObjectID arg_list_2    = necro_create_list_node(&runtime, (NecroListNode) { constant_3, NULL_NECRO_OBJECT_ID });
        NecroObjectID arg_list_1    = necro_create_list_node(&runtime, (NecroListNode) { constant_2, arg_list_2 });
        NecroObjectID current_value = necro_create_int(&runtime, 0);
        NecroObjectID app           = necro_create_app(&runtime, (NecroApp) { current_value, plus, arg_list_1, 2 });
        NecroObjectID env           = necro_create_env(&runtime, (NecroEnv) { NULL_NECRO_OBJECT_ID, -1, NULL_NECRO_OBJECT_ID });
        NecroObjectID result        = necro_eval(&runtime, env, app);

        necro_test_expr_eval(&runtime, "eval 1", result, NECRO_OBJECT_INT, 5);

        // NOTE: Looks like Necronomicon is about 40x slower than C at the moment.
        // For comparison python is around 30x slower than C
        // Necronomicon benchmark
        double start_time = (double) clock() / (double) CLOCKS_PER_SEC;
        int64_t iterations  = 10000;
        int64_t accumulator = 0;
        for (size_t i = 0; i < iterations; ++i)
        {
            result       = necro_eval(&runtime, env, app);
            accumulator += runtime.objects[result.id].int_value;
        }
        double  end_time    = (double) clock() / (double) CLOCKS_PER_SEC;
        double  run_time    = end_time - start_time;
        int64_t ns_per_iter = (int64_t) ((run_time / (double)iterations) * 1000000000);
        puts("");
        printf("eval 1 Necronomicon benchmark:\n    iterations:  %lld\n    run_time:    %f\n    ns/iter:     %lld\n    result:      %lld\n", iterations, run_time, ns_per_iter, accumulator);

        // Pure C benchmark
        start_time  = (double) clock() / (double) CLOCKS_PER_SEC;
        accumulator = 0;
        for (size_t i = 0; i < iterations; ++i)
        {
            accumulator += 2 + 3;
        }
        end_time    = (double) clock() / (double) CLOCKS_PER_SEC;
        run_time    = end_time - start_time;
        ns_per_iter = (int64_t) ((run_time / (double)iterations) * 1000000000);
        puts("");
        printf("eval 1 C benchmark:\n    iterations:  %lld\n    run_time:    %f\n    ns/iter:     %lld\n    result:      %lld\n", iterations, run_time, ns_per_iter, accumulator);

        necro_destroy_runtime(&runtime);
    }

    // Eval 2:
    // (\x y -> x + y) 6 7
    // translates to:
    // (App 0 (lambda (App 0 (+) ((Var x) (Var y) ()) 2) Env 2) (6 7 ()) 2)
    {
        puts("\n------\neval2:\n");
        // Initialize Runtime
        runtime = necro_create_runtime((NecroAudioInfo) { 44100, 512 });

        // Lambda
        NecroObjectID var_x        = necro_create_var(&runtime, (NecroVar) { 0, NULL_NECRO_OBJECT_ID });
        NecroObjectID var_y        = necro_create_var(&runtime, (NecroVar) { 1, NULL_NECRO_OBJECT_ID });
        NecroObjectID add          = necro_create_primop(&runtime, (NecroPrimOp) { NECRO_PRIM_ADD_I });
        NecroObjectID l_arg_list_2 = necro_create_list_node(&runtime, (NecroListNode) { var_y, NULL_NECRO_OBJECT_ID });
        NecroObjectID l_arg_list_1 = necro_create_list_node(&runtime, (NecroListNode) { var_x, l_arg_list_2 });
        NecroObjectID inner_curr   = necro_create_int(&runtime, 0);
        NecroObjectID lam_app      = necro_create_app(&runtime, (NecroApp) { inner_curr, add, l_arg_list_1, 2 });
        NecroObjectID lam_env_2    = necro_create_env(&runtime, (NecroEnv) { NULL_NECRO_OBJECT_ID, 1, NULL_NECRO_OBJECT_ID });
        NecroObjectID lam_env_1    = necro_create_env(&runtime, (NecroEnv) { lam_env_2, 0, NULL_NECRO_OBJECT_ID });
        NecroObjectID lambda       = necro_create_lambda(&runtime, (NecroLambda) { lam_app, lam_env_1, NULL_NECRO_OBJECT_ID, 2 });

        // App
        NecroObjectID constant_6   = necro_create_int(&runtime, 6);
        NecroObjectID constant_7   = necro_create_int(&runtime, 7);
        NecroObjectID o_arg_list_2 = necro_create_list_node(&runtime, (NecroListNode) { constant_7, NULL_NECRO_OBJECT_ID });
        NecroObjectID o_arg_list_1 = necro_create_list_node(&runtime, (NecroListNode) { constant_6, o_arg_list_2 });
        NecroObjectID outter_curr  = necro_create_int(&runtime, 0);
        NecroObjectID outter_app   = necro_create_app(&runtime, (NecroApp) { outter_curr, lambda, o_arg_list_1, 2 });
        NecroObjectID outter_env   = necro_create_env(&runtime, (NecroEnv) { NULL_NECRO_OBJECT_ID, -1, NULL_NECRO_OBJECT_ID });
        NecroObjectID result       = necro_eval(&runtime, outter_env, outter_app);

        necro_test_expr_eval(&runtime, "eval 2", result, NECRO_OBJECT_INT, 13);

        // NOTE: Looks like Necronomicon is about 40x slower than C at the moment.
        // For comparison python is around 30x slower than C
        // Necronomicon benchmark
        double start_time = (double) clock() / (double) CLOCKS_PER_SEC;
        int64_t iterations  = 10000;
        int64_t accumulator = 0;
        for (size_t i = 0; i < iterations; ++i)
        {
            result       = necro_eval(&runtime, outter_env, outter_app);
            accumulator += runtime.objects[result.id].int_value;
        }
        double  end_time    = (double) clock() / (double) CLOCKS_PER_SEC;
        double  run_time    = end_time - start_time;
        int64_t ns_per_iter = (int64_t) ((run_time / (double)iterations) * 1000000000);
        puts("");
        printf("eval 2 Necronomicon benchmark:\n    iterations:  %lld\n    run_time:    %f\n    ns/iter:     %lld\n    result:      %lld\n", iterations, run_time, ns_per_iter, accumulator);

        necro_destroy_runtime(&runtime);
    }

    // Eval 3:
    // (\x -> x2 + y where x2 = x + x; y = x) 10
    {
        puts("\n------\neval3:\n");
        // Initialize Runtime
        runtime = necro_create_runtime((NecroAudioInfo) { 44100, 512 });

        // X
        NecroObjectID var_x        = necro_create_var(&runtime, (NecroVar) { 0, NULL_NECRO_OBJECT_ID });

        // y
        NecroObjectID var_y        = necro_create_var(&runtime, (NecroVar) { 2, NULL_NECRO_OBJECT_ID });

        // x2
        NecroObjectID var_x2       = necro_create_var(&runtime, (NecroVar) { 1, NULL_NECRO_OBJECT_ID });
        NecroObjectID add_x2       = necro_create_primop(&runtime, (NecroPrimOp) { NECRO_PRIM_ADD_I });
        NecroObjectID x_arg_list_2 = necro_create_list_node(&runtime, (NecroListNode) { var_x, NULL_NECRO_OBJECT_ID });
        NecroObjectID x_arg_list_1 = necro_create_list_node(&runtime, (NecroListNode) { var_x, x_arg_list_2 });
        NecroObjectID x_curr       = necro_create_int(&runtime, 0);
        NecroObjectID x_app        = necro_create_app(&runtime, (NecroApp) { x_curr, add_x2, x_arg_list_1, 2 });

        // where
        NecroObjectID where_list_2 = necro_create_list_node(&runtime, (NecroListNode) { var_x, NULL_NECRO_OBJECT_ID });
        NecroObjectID where_list_1 = necro_create_list_node(&runtime, (NecroListNode) { x_app, where_list_2 });

        // Lambda Body
        NecroObjectID add          = necro_create_primop(&runtime, (NecroPrimOp) { NECRO_PRIM_ADD_I });
        NecroObjectID l_arg_list_2 = necro_create_list_node(&runtime, (NecroListNode) { var_y,  NULL_NECRO_OBJECT_ID });
        NecroObjectID l_arg_list_1 = necro_create_list_node(&runtime, (NecroListNode) { var_x2, l_arg_list_2 });
        NecroObjectID lam_curr     = necro_create_int(&runtime, 0);
        NecroObjectID lam_app      = necro_create_app(&runtime, (NecroApp) { lam_curr, add, l_arg_list_1, 2 });

        // Lambda
        NecroObjectID lam_env_3    = necro_create_env(&runtime, (NecroEnv) { NULL_NECRO_OBJECT_ID, 2, NULL_NECRO_OBJECT_ID });
        NecroObjectID lam_env_2    = necro_create_env(&runtime, (NecroEnv) { lam_env_3, 1, NULL_NECRO_OBJECT_ID });
        NecroObjectID lam_env_1    = necro_create_env(&runtime, (NecroEnv) { lam_env_2, 0, NULL_NECRO_OBJECT_ID });
        NecroObjectID lambda       = necro_create_lambda(&runtime, (NecroLambda) { lam_app, lam_env_1, where_list_1, 1 });

        // App
        NecroObjectID constant_10    = necro_create_int(&runtime, 10);
        NecroObjectID app_arg_list_1 = necro_create_list_node(&runtime, (NecroListNode) { constant_10, NULL_NECRO_OBJECT_ID });
        NecroObjectID app_curr       = necro_create_int(&runtime, 0);
        NecroObjectID app_env        = necro_create_env(&runtime, (NecroEnv) { NULL_NECRO_OBJECT_ID, -1, NULL_NECRO_OBJECT_ID });
        NecroObjectID app_app        = necro_create_app(&runtime, (NecroApp) { app_curr, lambda, app_arg_list_1, 1 });

        NecroObjectID result         = necro_eval(&runtime, app_env, app_app);

        necro_test_expr_eval(&runtime, "eval 3", result, NECRO_OBJECT_INT, 30);
        necro_destroy_runtime(&runtime);
    }
}

//=====================================================
// VM
//=====================================================

// This is now more like 5.5 slower than C
// Stack machine with accumulator
uint64_t necro_run_vm(uint64_t* instructions, size_t heap_size)
{
    // Stack grows down
    register int64_t* sp  = malloc(NECRO_STACK_SIZE * sizeof(int64_t));
    register int64_t* pc  = instructions;
    register int64_t* fp  = 0;
    register int64_t  acc = 0;
    register int64_t  env = 0;
    sp = sp + (NECRO_STACK_SIZE - 1);
    pc--;
    while (true)
    {
        switch (*++pc)
        {

        // Integer operations
        case N_PUSH_I:
            DEBUG_PRINT("N_PUSH_I");
            *--sp = acc;
            acc = *++pc;
            break;
        case N_ADD_I:
            DEBUG_PRINT("N_ADD_I");
            acc = acc + *sp++;
            break;
        case N_SUB_I:
            DEBUG_PRINT("N_SUB_I");
            acc = acc - *sp++;
            break;
        case N_MUL_I:
            DEBUG_PRINT("N_MUL_I");
            acc = acc * *sp++;
            break;
        case N_NEG_I:
            DEBUG_PRINT("N_NEG_I");
            acc = -acc;
            break;
        case N_DIV_I:
            DEBUG_PRINT("N_DIV_I");
            acc = acc / *sp++;
            break;
        case N_MOD_I:
            DEBUG_PRINT("N_MOD_I");
            acc = acc % *sp++;
            break;
        case N_EQ_I:
            DEBUG_PRINT("N_EQ_I");
            acc = acc == *sp++;
            break;
        case N_NEQ_I:
            DEBUG_PRINT("N_NEQ_I");
            acc = acc != *sp++;
            break;
        case N_LT_I:
            DEBUG_PRINT("N_LT_I");
            acc = acc < *sp++;
            break;
        case N_LTE_I:
            DEBUG_PRINT("N_LTE_I");
            acc = acc <= *sp++;
            break;
        case N_GT_I:
            DEBUG_PRINT("N_GT_I");
            acc = acc > *sp++;
            break;
        case N_GTE_I:
            DEBUG_PRINT("N_GTE_I");
            acc = acc >= *sp++;
            break;
        case N_BIT_AND_I:
            DEBUG_PRINT("N_BIT_AND_I");
            acc = acc & *sp++;
            break;
        case N_BIT_OR_I:
            DEBUG_PRINT("N_BIT_OR_I");
            acc = acc | *sp++;
            break;
        case N_BIT_XOR_I:
            DEBUG_PRINT("N_BIT_XOR_I");
            acc = acc ^ *sp++;
            break;
        case N_BIT_LS_I:
            DEBUG_PRINT("N_BIT_LS_I");
            acc = acc << *sp++;
            break;
        case N_BIT_RS_I:
            DEBUG_PRINT("N_BIT_RS_I");
            acc = acc >> *sp++;
            break;


        // Function Applications
        case N_APPLY_1:
        {
            DEBUG_PRINT("N_APPLY_1");
            int64_t arg_1 = sp[0];
            sp -= 3;
            sp[0] = arg_1;        // Argument 1
            sp[1] = (int64_t) pc; // Return address...Continuation?
            sp[2] = env;          // Environemtn
            break;
        }

        // Jumping
        case N_JMP:
            DEBUG_PRINT("N_JMP");
            pc++;
            pc += *pc; // Using relative jumps
            break;
        case N_JMP_IF:
            DEBUG_PRINT("N_JMP_IF");
            pc++;
            if (acc)
                pc += *pc;
            acc = *sp--;
            break;
        case N_JMP_IF_NOT:
            DEBUG_PRINT("N_JMP_IF_NOT");
            pc++;
            if (!acc)
                pc += *pc;
            acc = *sp--;
            break;

        // Commands
        case N_POP:
            DEBUG_PRINT("N_POP");
            sp++;
            break;
        case N_PRINT:
            printf("PRINT: %lld\n", acc);
            break;
        case N_HALT:
            DEBUG_PRINT("N_HALT");
            return acc;
        default:
            printf("Unrecognized command: %lld\n", *pc);
            return acc;
        }
    }
}

void necro_test_vm_eval(int64_t* instr, int64_t result, const char* print_string)
{
    int64_t vm_result = necro_run_vm(instr, 0);
    if (vm_result == result)
        printf("%s passed\n", print_string);
    else
        printf("%s FAILED\n    expected result: %lld\n    vm result:       %lld\n\n", print_string, result, vm_result);
}

void necro_test_vm()
{
    puts("\n------");
    puts("Test VM\n");

    // Integer tests
    {
        int64_t instr[6] =
        {
            N_PUSH_I, 7,
            N_PUSH_I, 6,
            N_ADD_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 13, "AddI:   ");
    }

    {
        int64_t instr[6] =
        {
            N_PUSH_I, 4,
            N_PUSH_I, 9,
            N_SUB_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 5, "SubI:   ");
    }

    {
        int64_t instr[6] =
        {
            N_PUSH_I, 10,
            N_PUSH_I, 3,
            N_MUL_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 30, "MulI:   ");
    }

    {
        int64_t instr[4] =
        {
            N_PUSH_I,     10,
            N_NEG_I,
            N_HALT
        };
        necro_test_vm_eval(instr, -10, "NegI:   ");
    }

    {
        int64_t instr[6] =
        {
            N_PUSH_I, 10,
            N_PUSH_I, 1000,
            N_DIV_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 100, "DivI:   ");
    }

    {
        int64_t instr[6] =
        {
            N_PUSH_I, 4,
            N_PUSH_I, 5,
            N_MOD_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 1, "ModI:   ");
    }

    {
        int64_t instr[6] =
        {
            N_PUSH_I, 666,
            N_PUSH_I, 555,
            N_EQ_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 0, "EqI:    ");
    }

    {
        int64_t instr[6] =
        {
            N_PUSH_I, 666,
            N_PUSH_I, 555,
            N_NEQ_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 1, "NEqI:   ");
    }

    {
        int64_t instr[6] =
        {
            N_PUSH_I, 1,
            N_PUSH_I, 2,
            N_LT_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 0, "LTI:    ");
    }

    {
        int64_t instr[6] =
        {
            N_PUSH_I, 3,
            N_PUSH_I, 3,
            N_LTE_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 1, "LTEI:   ");
    }

    {
        int64_t instr[6] =
        {
            N_PUSH_I, 100,
            N_PUSH_I, 200,
            N_GT_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 1, "GTI:    ");
    }

    {
        int64_t instr[6] =
        {
            N_PUSH_I, 100,
            N_PUSH_I, 99,
            N_GTE_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 0, "GTEI:   ");
    }

    {
        int64_t instr[6] =
        {
            N_PUSH_I, 7,
            N_PUSH_I, 6,
            N_BIT_AND_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 6 & 7, "BAND:   ");
    }

    {
        int64_t instr[6] =
        {
            N_PUSH_I, 13,
            N_PUSH_I, 4,
            N_BIT_OR_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 4 | 13, "BOR:    ");
    }

    {
        int64_t instr[6] =
        {
            N_PUSH_I, 22,
            N_PUSH_I, 11,
            N_BIT_XOR_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 11 ^ 22, "BXOR:   ");
    }

    {
        int64_t instr[6] =
        {
            N_PUSH_I, 2,
            N_PUSH_I, 4,
            N_BIT_LS_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 4 << 2, "BLS:    ");
    }

    {
        int64_t instr[6] =
        {
            N_PUSH_I, 1,
            N_PUSH_I, 5,
            N_BIT_RS_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 5 >> 1, "BRS:    ");
    }

    // Jump tests
    {
        int64_t instr[10] =
        {
            N_PUSH_I, 6,
            N_JMP, 2,
            N_PUSH_I, 7,
            N_PUSH_I, 8,
            N_ADD_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 14, "Jmp:    ");
    }


    {
        int64_t instr[14] =
        {
            N_PUSH_I, 3,
            N_PUSH_I, 1,
            N_JMP_IF, 4,
            N_PUSH_I, 4,
            N_JMP,    2,
            N_PUSH_I, 5,
            N_ADD_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 8, "JmpIf 1:");
    }

    {
        int64_t instr[14] =
        {
            N_PUSH_I, 3,
            N_PUSH_I, 0,
            N_JMP_IF, 4,
            N_PUSH_I, 4,
            N_JMP,    2,
            N_PUSH_I, 5,
            N_ADD_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 7, "JmpIf 2:");
    }

    {
        int64_t instr[14] =
        {
            N_PUSH_I,     10,
            N_PUSH_I,     1,
            N_JMP_IF_NOT, 4,
            N_PUSH_I,     20,
            N_JMP,        2,
            N_PUSH_I,     50,
            N_SUB_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 10, "JmpIfN1:");
    }

    {
        int64_t instr[14] =
        {
            N_PUSH_I,     10,
            N_PUSH_I,     0,
            N_JMP_IF_NOT, 4,
            N_PUSH_I,     40,
            N_JMP,        2,
            N_PUSH_I,     50,
            N_SUB_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 40, "JmpIfN2:");
    }

}
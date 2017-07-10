/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <string.h>
#include "runtime.h"

// Trying out a 4 : 1 ratio of language objects per audio block for now
#define NECRO_INITIAL_NUM_OBJECT_SLABS  16384
#define NECRO_INITIAL_NUM_AUDIO_BUFFERS 4096

//=====================================================
// Initialization And Cleanup
//=====================================================
NecroRuntime necro_create_runtime(NecroAudioInfo audio_info)
{
    // Allocate all runtime memory as a single contiguous chunk
    // NOTE: Is this is a good idea? It means we can't grow the memory at runtime if we run out.....
    // Do we want to be more draconian or forgiving about this?
    size_t size_of_objects         = NECRO_INITIAL_NUM_OBJECT_SLABS  * sizeof(NecroObject);
    size_t size_of_audio_free_list = NECRO_INITIAL_NUM_AUDIO_BUFFERS * sizeof(uint32_t);
    size_t size_of_audio           = NECRO_INITIAL_NUM_AUDIO_BUFFERS * (audio_info.block_size * sizeof(double));
    char*  runtime_memory          = malloc(size_of_objects + size_of_audio + size_of_audio_free_list);
    if (runtime_memory == NULL)
    {
        fprintf(stderr, "Malloc returned NULL when allocating memory for runtime in necro_create_runtime().");
        exit(1);
    }

    // Initialize NecroObject slabs
    NecroObject* objects = (NecroObject*)runtime_memory;
    objects[0].type      = NECRO_OBJECT_NULL;
    for (uint32_t i = 1; i < NECRO_INITIAL_NUM_OBJECT_SLABS; ++i)
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

inline NecroObject* necro_get_object(NecroRuntime* runtime, NecroObjectID object_id)
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

//=====================================================
// PrimOps
//=====================================================
inline void necro_run_prim_op_code(NecroRuntime* runtime, NecroApp app, NecroPrimOp primop)
{
    switch (primop.op)
    {
    case ADD_I:
        {
            NecroObject* arg_list_node_1 = necro_get_object(runtime, app.argument_list_id);
            NecroObject* arg_list_node_2 = necro_get_object(runtime, arg_list_node_1->list_node.next_id);
            NecroObject* arg_1           = necro_get_object(runtime, arg_list_node_1->list_node.value_id);
            NecroObject* arg_2           = necro_get_object(runtime, arg_list_node_2->list_node.value_id);
            NecroObject* current_value   = necro_get_object(runtime, app.current_value_id);
            assert(arg_list_node_1 != NULL);
            assert(arg_list_node_2 != NULL);
            assert(arg_1 != NULL);
            assert(arg_2 != NULL);
            // TODO: evaluate arg_1 and arg_2 here
            int64_t result = 0;
            current_value->int_value = result;
        }
        break;
    }
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
    necro_run_test(var_object != NULL && var_object->type == NECRO_OBJECT_VAR && var_object->var.var_symbol == 0 && var_object->var.value_id == 1, "NecroRuntime create var test:   ");

    // Create App
    NecroObjectID app        = necro_create_app(&runtime, (NecroApp) { 1, 2, 3, 4 });
    NecroObject*  app_object = necro_get_object(&runtime, app);
    necro_run_test(app_object != NULL && app_object->type == NECRO_OBJECT_APP && app_object->app.current_value_id.id == 1 && app_object->app.lambda_id.id == 2 && app_object->app.argument_list_id.id == 3 && app_object->app.argument_count == 4, "NecroRuntime create app test:   ");

    // Create Papp
    NecroObjectID pap        = necro_create_pap(&runtime, (NecroPap) { 2, 3, 4, 5 });
    NecroObject*  pap_object = necro_get_object(&runtime, pap);
    necro_run_test(pap_object != NULL && pap_object->type == NECRO_OBJECT_PAP && pap_object->pap.current_value_id.id == 2 && pap_object->pap.lambda_id.id == 3 && pap_object->pap.argument_list_id.id == 4 && pap_object->pap.current_arg_count == 5, "NecroRuntime create pap test:   ");

    // Create Lambda
    NecroObjectID lam        = necro_create_lambda(&runtime, (NecroLambda) { 3, 4, 5, 6 });
    NecroObject*  lam_object = necro_get_object(&runtime, lam);
    necro_run_test(lam_object != NULL && lam_object->type == NECRO_OBJECT_LAMBDA && lam_object->lambda.current_value_id.id == 3 && lam_object->lambda.body_id.id == 4 && lam_object->lambda.env_id.id == 5 && lam_object->lambda.arity == 6, "NecroRuntime create lam test:   ");

    // Create PrimOp
    NecroObjectID pri        = necro_create_primop(&runtime, (NecroPrimOp) { 4, 5, 6, 7 });
    NecroObject*  pri_object = necro_get_object(&runtime, pri);
    necro_run_test(pri_object != NULL && pri_object->type == NECRO_OBJECT_PRIMOP && pri_object->primop.current_value_id.id == 4 && pri_object->primop.op == 5 && pri_object->primop.env_id.id == 6 && pri_object->primop.arity == 7, "NecroRuntime create primop test:");

    // Create Env
    NecroObjectID env        = necro_create_env(&runtime, (NecroEnv) { 5, 6, 7, 8 });
    NecroObject*  env_object = necro_get_object(&runtime, env);
    necro_run_test(env_object != NULL && env_object->type == NECRO_OBJECT_ENV && env_object->env.parent_env.id == 5 && env_object->env.next_env_node.id == 6 && env_object->env.var_symbol == 7 && env_object->env.value_id.id == 8, "NecroRuntime create env test:   ");

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

    // Destroy Runtime
    necro_destroy_runtime(&runtime);
    necro_run_test(runtime.audio == NULL && runtime.objects == NULL && runtime.audio_free_list == NULL, "NecroRuntime destroy test:      ");
}
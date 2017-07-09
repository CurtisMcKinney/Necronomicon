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
uint32_t necro_alloc_audio(NecroRuntime* runtime)
{
    uint32_t current_free = runtime->audio_free_list[runtime->audio_free_list_head];
    if (current_free == 0)
    {
        fprintf(stderr, "Runtime error: Runtime memory exhausted while allocating audio block!\n");
        exit(1);
    }
    runtime->audio_free_list_head--;
    return current_free;
}

void necro_free_audio(NecroRuntime* runtime, uint32_t audio_id)
{
    runtime->audio_free_list_head++;
    runtime->audio_free_list[runtime->audio_free_list_head] = audio_id;
}

uint32_t necro_alloc_object(NecroRuntime* runtime)
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
    return current_free;
}

void necro_free_object(NecroRuntime* runtime, uint32_t object_id)
{
    runtime->objects[object_id].type            = NECRO_OBJECT_FREE;
    runtime->objects[object_id].next_free_index = runtime->object_free_list;
    runtime->object_free_list                   = object_id;
}

NecroObject* necro_get_object(NecroRuntime* runtime, uint32_t object_id)
{
    // Higher bounds check?
    if (object_id == 0)
        return NULL;
    return &runtime->objects[object_id];
}

uint32_t necro_create_var(NecroRuntime* runtime, NecroVar var)
{
    uint32_t id               = necro_alloc_object(runtime);
    runtime->objects[id].type = NECRO_OBJECT_VAR;
    runtime->objects[id].var  = var;
    return id;
}

uint32_t necro_create_app(NecroRuntime* runtime, NecroApp app)
{
    uint32_t id               = necro_alloc_object(runtime);
    runtime->objects[id].type = NECRO_OBJECT_APP;
    runtime->objects[id].app  = app;
    return id;
}

uint32_t necro_create_pap(NecroRuntime* runtime, NecroPap pap)
{
    uint32_t id               = necro_alloc_object(runtime);
    runtime->objects[id].type = NECRO_OBJECT_PAP;
    runtime->objects[id].pap  = pap;
    return id;
}

uint32_t necro_create_lambda(NecroRuntime* runtime, NecroLambda lambda)
{
    uint32_t id                 = necro_alloc_object(runtime);
    runtime->objects[id].type   = NECRO_OBJECT_LAMBDA;
    runtime->objects[id].lambda = lambda;
    return id;
}

uint32_t necro_create_primop(NecroRuntime* runtime, NecroPrimOp primop)
{
    uint32_t id                 = necro_alloc_object(runtime);
    runtime->objects[id].type   = NECRO_OBJECT_PRIMOP;
    runtime->objects[id].primop = primop;
    return id;
}

uint32_t necro_create_env(NecroRuntime* runtime, NecroEnv env)
{
    uint32_t id               = necro_alloc_object(runtime);
    runtime->objects[id].type = NECRO_OBJECT_ENV;
    runtime->objects[id].env  = env;
    return id;
}

uint32_t necro_create_float(NecroRuntime* runtime, double value)
{
    uint32_t id                      = necro_alloc_object(runtime);
    runtime->objects[id].type        = NECRO_OBJECT_FLOAT;
    runtime->objects[id].float_value = value;
    return id;
}

uint32_t necro_create_int(NecroRuntime* runtime, int64_t value)
{
    uint32_t id                    = necro_alloc_object(runtime);
    runtime->objects[id].type      = NECRO_OBJECT_INT;
    runtime->objects[id].int_value = value;
    return id;
}

uint32_t necro_create_char(NecroRuntime* runtime, char value)
{
    uint32_t id                     = necro_alloc_object(runtime);
    runtime->objects[id].type       = NECRO_OBJECT_CHAR;
    runtime->objects[id].char_value = value;
    return id;
}

uint32_t necro_create_bool(NecroRuntime* runtime, bool value)
{
    uint32_t id                     = necro_alloc_object(runtime);
    runtime->objects[id].type       = NECRO_OBJECT_BOOL;
    runtime->objects[id].bool_value = value;
    return id;
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
    uint32_t object_id = necro_alloc_object(&runtime);
    necro_run_test(runtime.objects[object_id].type == NECRO_OBJECT_NULL && runtime.object_free_list != object_id && runtime.objects[object_id].ref_count == 0, "NecroRuntime alloc object test: ");

    // Free Object
    necro_free_object(&runtime, object_id);
    necro_run_test(runtime.objects[object_id].type == NECRO_OBJECT_FREE && runtime.object_free_list == object_id, "NecroRuntime free object test:  ");

    // Alloc Audio
    uint32_t audio_id = necro_alloc_audio(&runtime);
    necro_run_test(runtime.audio_free_list[runtime.audio_free_list_head] != audio_id && audio_id != 0, "NecroRuntime alloc audio test:  ");

    // Free Audio
    necro_free_audio(&runtime, audio_id);
    necro_run_test(runtime.audio_free_list[runtime.audio_free_list_head] == audio_id, "NecroRuntime free object test:  ");

    // Create Var
    uint32_t     var        = necro_create_var(&runtime, (NecroVar) { 0, 1 });
    NecroObject* var_object = necro_get_object(&runtime, var);
    necro_run_test(var_object != NULL && var_object->type == NECRO_OBJECT_VAR && var_object->var.var_symbol == 0 && var_object->var.value_id == 1, "NecroRuntime create var test:   ");

    // Create App
    uint32_t     app        = necro_create_app(&runtime, (NecroApp) { 1, 2, 3, 4 });
    NecroObject* app_object = necro_get_object(&runtime, app);
    necro_run_test(app_object != NULL && app_object->type == NECRO_OBJECT_APP && app_object->app.current_value_id == 1 && app_object->app.lambda_id == 2 && app_object->app.argument_1_id == 3 && app_object->app.argument_count == 4, "NecroRuntime create app test:   ");

    // Create Papp
    uint32_t     pap        = necro_create_pap(&runtime, (NecroPap) { 2, 3, 4, 5 });
    NecroObject* pap_object = necro_get_object(&runtime, pap);
    necro_run_test(pap_object != NULL && pap_object->type == NECRO_OBJECT_PAP && pap_object->pap.current_value_id == 2 && pap_object->pap.lambda_id == 3 && pap_object->pap.argument_1_id == 4 && pap_object->pap.current_arg_count == 5, "NecroRuntime create pap test:   ");

    // Create Lambda
    uint32_t     lam        = necro_create_lambda(&runtime, (NecroLambda) { 3, 4, 5, 6 });
    NecroObject* lam_object = necro_get_object(&runtime, lam);
    necro_run_test(lam_object != NULL && lam_object->type == NECRO_OBJECT_LAMBDA && lam_object->lambda.current_value_id == 3 && lam_object->lambda.body_id == 4 && lam_object->lambda.env_id == 5 && lam_object->lambda.arity == 6, "NecroRuntime create lam test:   ");

    // Create PrimOp
    uint32_t     pri        = necro_create_primop(&runtime, (NecroPrimOp) { 4, 5, 6, 7 });
    NecroObject* pri_object = necro_get_object(&runtime, pri);
    necro_run_test(pri_object != NULL && pri_object->type == NECRO_OBJECT_PRIMOP && pri_object->primop.current_value_id == 4 && pri_object->primop.op == 5 && pri_object->primop.env_id == 6 && pri_object->primop.arity == 7, "NecroRuntime create primop test:");

    // Create Env
    uint32_t     env        = necro_create_env(&runtime, (NecroEnv) { 5, 6, 7, 8 });
    NecroObject* env_object = necro_get_object(&runtime, env);
    necro_run_test(env_object != NULL && env_object->type == NECRO_OBJECT_ENV && env_object->env.parent_env == 5 && env_object->env.next_env_node == 6 && env_object->env.var_symbol == 7 && env_object->env.value_id == 8, "NecroRuntime create env test:   ");

    // Create Float
    uint32_t     f            = necro_create_float(&runtime, 100.5);
    NecroObject* float_object = necro_get_object(&runtime, f);
    necro_run_test(float_object != NULL && float_object->type == NECRO_OBJECT_FLOAT && float_object->float_value == 100.5, "NecroRuntime create float test: ");

    // Create Int
    uint32_t     i          = necro_create_int(&runtime, 666);
    NecroObject* int_object = necro_get_object(&runtime, i);
    necro_run_test(int_object != NULL && int_object->type == NECRO_OBJECT_INT && int_object->int_value == 666, "NecroRuntime create int test:   ");

    // Create Char
    uint32_t     c           = necro_create_char(&runtime, 'N');
    NecroObject* char_object = necro_get_object(&runtime, c);
    necro_run_test(char_object != NULL && char_object->type == NECRO_OBJECT_CHAR && char_object->char_value == 'N', "NecroRuntime create char test:  ");

    // Create Bool
    uint32_t     b           = necro_create_bool(&runtime, true);
    NecroObject* bool_object = necro_get_object(&runtime, b);
    necro_run_test(bool_object != NULL && bool_object->type == NECRO_OBJECT_BOOL && bool_object->bool_value == true, "NecroRuntime create bool test:  ");

    // Destroy Runtime
    necro_destroy_runtime(&runtime);
    necro_run_test(runtime.audio == NULL && runtime.objects == NULL && runtime.audio_free_list == NULL, "NecroRuntime destroy test:      ");
}
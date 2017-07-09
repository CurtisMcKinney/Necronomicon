/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <string.h>
#include "runtime.h"

#define NECRO_INITIAL_NUM_OBJECT_SLABS  16384
#define NECRO_INITIAL_NUM_AUDIO_BUFFERS 4096

NecroRuntime necro_create_runtime(NecroAudioInfo audio_info)
{
    // Allocate all runtime memory as a single contiguous chunk
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

void necro_test_runtime()
{
    puts("--------------------------------");
    puts("-- Testing NecroRuntime");
    puts("--------------------------------\n");

    // Initialize
    NecroRuntime runtime = necro_create_runtime((NecroAudioInfo) { 44100, 512 });
    if (runtime.audio != NULL && runtime.objects != NULL)
        puts("NecroRuntime alloc test:   passed");
    else
        puts("NecroRuntime alloc test:   failed");

    // Destroy
    necro_destroy_runtime(&runtime);
    if (runtime.audio == NULL && runtime.objects == NULL && runtime.audio_free_list == NULL)
        puts("NecroRuntime destroy test: passed");
    else
        puts("NecroRuntime destroy test: failed");
}

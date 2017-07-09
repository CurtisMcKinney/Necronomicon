/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <string.h>
#include "runtime.h"

#define NECRO_INITIAL_NUM_OBJECT_SLABS  16384
#define NECRO_INITIAL_NUM_AUDIO_BUFFERS 4096

NecroRuntime necro_create_runtime(NecroAudioInfo audio_info)
{
    // Allocate all runtime memory as a single contiguous chunk
    size_t size_of_objects = NECRO_INITIAL_NUM_OBJECT_SLABS  * sizeof(NecroObject);
    size_t size_of_audio   = NECRO_INITIAL_NUM_AUDIO_BUFFERS * (audio_info.block_size * sizeof(double));
    char*  runtime_memory  = malloc(size_of_objects + size_of_audio);

    if (runtime_memory == NULL)
    {
        fprintf(stderr, "Malloc returned NULL when allocating memory for runtime in necro_create_runtime().");
        exit(1);
    }

    // NecroObject slabs
    NecroObject* objects = (NecroObject*)runtime_memory;

    // Initialize NecroObject slabs as a free list
    objects[0].type = NECRO_OBJECT_NULL;
    for (uint32_t i = 1; i < NECRO_INITIAL_NUM_OBJECT_SLABS; ++i)
    {
        objects[i].next_free_index = i - 1;
        objects[i].ref_count       = 0;
        objects[i].type            = NECRO_OBJECT_FREE;
    }

    // Warning: Black Magic
    // Audio buffer is essentially a union between audio blocks and a free list of open audio buffers
    // Take the address of the audio member of the NecroAudioBlock struct to get a pointer to the audio buffer.
    NecroAudioBlock* audio = (NecroAudioBlock*)(runtime_memory + size_of_objects);

    // Initialize Audio
    for (size_t i = 1; i < NECRO_INITIAL_NUM_AUDIO_BUFFERS; ++i)
    {
        size_t offset = i * (audio_info.block_size * sizeof(double));
        audio[offset] = (NecroAudioBlock) { { (i - 1) * (audio_info.block_size * sizeof(double)) } }; // TODO: Fix
    }

    // return NecroRuntime struct
    return (NecroRuntime)
    {
        objects,
        NECRO_INITIAL_NUM_OBJECT_SLABS - 1,
        audio,
        NECRO_INITIAL_NUM_AUDIO_BUFFERS - 1
    };
}

void necro_destroy_runtime(NecroRuntime* runtime)
{
    if (runtime->audio != NULL)
    {
        free(runtime->audio);
        runtime->audio = NULL;
    }
    if (runtime->objects != NULL)
    {
        free(runtime->objects);
        runtime->objects = NULL;
    }
    runtime->object_free_list = 0;
}

void necro_test_runtime()
{
    puts("--------------------------------");
    puts("-- Testing NecroRuntime");
    puts("--------------------------------\n");

    // Initialize
    NecroRuntime runtime = necro_create_runtime((NecroAudioInfo) { 44100, 512 });
    if (runtime.audio != NULL && runtime.objects != NULL)
        puts("NecroRuntime alloc test:       passed");
    else
        puts("NecroRuntime alloc test:       failed");

    // Destroy
    necro_destroy_runtime(&runtime);
    if (runtime.audio == NULL && runtime.objects == NULL)
        puts("NecroRuntime destroy test:       passed");
    else
        puts("NecroRuntime destroy test:       failed");
}

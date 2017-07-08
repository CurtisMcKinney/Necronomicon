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
    // Allocate NecroObject slabs
    NecroObject* objects = malloc(NECRO_INITIAL_NUM_OBJECT_SLABS * sizeof(NecroObject));
    if (objects == NULL)
    {
        fprintf(stderr, "Malloc returned NULL when allocating memory for NecroObject slabs in necro_create_runtime().");
        exit(1);
    }

    // Initialize NecroObject slabs as a free list
    objects[0].type = NECRO_OBJECT_NULL;
    for (uint32_t i = 1; i < NECRO_INITIAL_NUM_OBJECT_SLABS; ++i)
    {
        objects[i].next_free_index = i - 1;
        objects[i].time            = 0;
        objects[i].ref_count       = 0;
        objects[i].type            = NECRO_OBJECT_FREE;
    }

    // return NecroRuntime struct
    return (NecroRuntime)
    {
        objects,
        NECRO_INITIAL_NUM_OBJECT_SLABS - 1,
        audio_info
    };
}
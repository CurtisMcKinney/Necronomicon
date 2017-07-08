/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef RUNTIME_H
#define RUNTIME_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

//=====================================================
// Theoretical Necro Runtime structs
//=====================================================
typedef enum
{
    NECRO_OBJECT_NULL,
    NECRO_OBJECT_BOOL,
    NECRO_OBJECT_FLOAT,
    NECRO_OBJECT_PAIR
} NECRO_OBJECT_TYPE;

typedef struct NecroObject NecroObject;

typedef struct
{
    float        value;
    NecroObject* next;
} NecroFloat;

typedef struct
{
    bool         value;
    NecroObject* next;
} NecroBool;

typedef struct
{
    NecroObject* first;
    NecroObject* second;
} NecroPair;

struct NecroObject
{
    union
    {
        NecroFloat necro_float;
        NecroBool  necro_bool;
    };
    uint32_t          ref_count;
    NECRO_OBJECT_TYPE type;
};

#endif // RUNTIME_H

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef RUNTIME_MOUSE_H
#define RUNTIME_MOUSE_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

// TODO: easy IO Module which export top level IO stream values

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

///////////////////////////////////////////////////////
// Necro Runtime functions
///////////////////////////////////////////////////////
extern DLLEXPORT int64_t _necro_mouse_x();
extern DLLEXPORT int64_t _necro_mouse_y();

///////////////////////////////////////////////////////
// Utility functions
///////////////////////////////////////////////////////
void necro_init_mouse();
void necro_poll_mouse();


#endif // RUNTIME_MOUSE_H
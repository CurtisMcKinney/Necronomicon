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

#include "runtime.h"

void   necro_init_mouse();
void   necro_poll_mouse();
extern DLLEXPORT int64_t _necro_mouse_x();
extern DLLEXPORT int64_t _necro_mouse_y();

#endif // RUNTIME_MOUSE_H
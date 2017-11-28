/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <string.h>
#include <stdio.h>
#include "utility.h"

void print_white_space(size_t white_count)
{
    for (size_t i = 0; i < white_count; ++i)
        printf(" ");
}

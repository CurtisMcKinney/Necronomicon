/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef UNICODE_PROPERTIES_H
#define UNICODE_PROPERTIES_H 1

#define UNICODE 1
#define _UNICODE 1

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "utility.h"

NECRO_RETURN_CODE necro_unicode_property_parse(const char* directory_name);
bool              necro_is_alphabetical(uint32_t code_point);
bool              necro_is_control(uint32_t code_point);
bool              necro_is_lowercase(uint32_t code_point);
bool              necro_is_uppercase(uint32_t code_point);
bool              necro_is_id_start(uint32_t code_point);
bool              necro_is_id_continue(uint32_t code_point);
bool              necro_is_ascii_digit(uint32_t code_point);
bool              necro_is_decimal(uint32_t code_point);
bool              necro_is_grapheme_base(uint32_t code_point);
bool              necro_is_whitespace(uint32_t code_point);
void              necro_test_unicode_properties();

// void              necro_printf(const char* )

#endif // UNICODE_PROPERTIES_H

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <string.h>
#include "utility.h"

char* necro_dup_string_slice(NecroStringSlice slice)
{
	if (slice.data == NULL)
		return NULL;
	// If the slice contains a null terminated string, do simple strndup
	if (slice.data[slice.length - 1] == '\0')
	{
		return strdup(slice.data);
	}
	// Else allocate a new string with an added character for the null byte
	else
	{
		char* new_str = malloc(slice.length + 1);
		if (!new_str)
			return NULL;
		memcpy(new_str, slice.data, slice.length);
		new_str[slice.length] = '\0';
		return new_str;
	}
}

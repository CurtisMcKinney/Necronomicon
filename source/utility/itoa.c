#include "itoa.h"

/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 *  with the distribution.
 *   * Neither the name of The Linux Foundation nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <assert.h>

// Windows //////////////////////////////////////////////////////////////////////////
#if defined(_WIN32) || defined(WIN32) || defined(_WIN64)
#include <stdlib.h>
char* necro_itoa(int num, char* str, size_t len, int base)
{
    assert(len > 0 && base > 2 && base < 37);
    return itoa(num, str, base);
}

#else // Non-Windows ////////////////////////////////////////////////////////////////

// slightly modified itoa from the linux foundation, per copyright above

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

char* strrev(char* str)
{
    if (! str || ! *str)
        return str;

    char* p1;
    char* p2; 
    for (p1 = str, p2 = str + strlen(str) - 1; p2 > p1; ++p1, --p2)
    {
        char temp = *p1;
        *p1 = *p2;
        *p2 = temp;
    }
    return str;
}

char* necro_itoa(int num, char* str, size_t len, int base)
{
    assert(len > 0 && base > 2 && base < 37);
	int sum = num;
	size_t i = 0;
	int digit;
	if (len == 0)
		return NULL;

	do
	{
		digit = sum % base;
		if (digit < 0xA)
			str[i++] = '0' + digit;
		else
			str[i++] = 'A' + digit - 0xA;
		sum /= base;
	} while (sum && (i < (len - 1)));

	if (i == (len - 1) && sum)
		return NULL;

	str[i] = '\0';
	strrev(str);
	return str;
}
#endif


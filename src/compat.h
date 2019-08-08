/*
 *******************************************************************
 *
 * Copyright 2016 Intel Corporation All rights reserved.
 *
 *-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
 */

#ifndef _COMPAT_H
#define _COMPAT_H

#include <string.h>
#include <stdint.h>
#include <safe_lib.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Code required for platform compatibility
 */

#if defined(__GNUC__) || defined(__MINGW64__)
#define THREAD __thread
#define BSWAP_32(n)  __builtin_bswap32(n)
#define BSWAP_64(n)  __builtin_bswap64(n)
#elif defined(_MSC_VER)
#define THREAD __declspec(thread)
#define BSWAP_32(n)  _byteswap_ulong(n)
#define BSWAP_64(n)  _byteswap_uint64(n)
#endif

#if defined(_WIN32)

#include <stdlib.h>

#define __LITTLE_ENDIAN   0
#define __BIG_ENDIAN      1
#define __BYTE_ORDER      __LITTLE_ENDIAN

static inline char* strndup(const char* str, size_t maxLen)
{
    size_t len = strnlen_s(str, RSIZE_MAX_STR);
    if (len > maxLen) {
        len = maxLen;
    }
    char* c = malloc(len + 1);
    if (c) {
        memcpy_s(c, len, str, len);
        c[len] = '\0';
    }
    return c;
}

#else /* posix */

#include <endian.h>

#endif

#ifdef __cplusplus
}
#endif

#endif

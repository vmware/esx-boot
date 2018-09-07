/*******************************************************************************
 * Copyright (c) 2008-2011,2015 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * stdint.h -- Integer types
 */

#ifndef _STDINT_H
#define _STDINT_H

#include <compiler.h>

#if defined(_MSC_VER)
typedef unsigned __int64         uint64_t;
typedef unsigned __int32         uint32_t;
typedef unsigned __int16         uint16_t;
typedef unsigned __int8          uint8_t;

typedef __int64                  int64_t;
typedef __int32                  int32_t;
typedef __int16                  int16_t;
typedef __int8                   int8_t;

#if defined(only_em64t) || defined(only_arm64)
typedef unsigned __int64         uintptr_t;
typedef signed __int64           intptr_t;
#else
typedef unsigned int             uintptr_t;
typedef signed int               intptr_t;
#endif

#elif defined(__GNUC__)
typedef unsigned long long int   uint64_t;
typedef unsigned int             uint32_t;
typedef unsigned short           uint16_t;
typedef unsigned char            uint8_t;

typedef signed long long int     int64_t;
typedef signed int               int32_t;
typedef signed short             int16_t;
typedef signed char              int8_t;

#if defined(only_em64t) || defined(only_arm64)
typedef unsigned long long int   uintptr_t;
typedef signed long long int     intptr_t;
#else
typedef unsigned int             uintptr_t;
typedef signed int               intptr_t;
#endif

#else
#error "Need compiler define"
#endif


typedef int64_t intmax_t;
typedef uint64_t uintmax_t;

#endif /* !_STDINT_H */

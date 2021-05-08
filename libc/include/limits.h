/*******************************************************************************
 * Copyright (c) 2008-2011,2015 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * limits.h
 */

#ifndef _LIMITS_H
#define _LIMITS_H

#define CHAR_BIT    8

#define UINT16_MAX  (65535U)

#define UINT32_MAX  (4294967295U)
#define INT32_MAX   (2147483647)
#define UINT_MAX    UINT32_MAX
#define INT_MAX     INT32_MAX

#if defined(only_em64t) || defined(only_arm64)
#define LONG_MIN    (-9223372036854775807L-1)
#define LONG_MAX    (9223372036854775807L)
#define ULONG_MAX   (18446744073709551615UL)
#else
#define LONG_MIN    (-2147483647L-1)
#define LONG_MAX    (2147483647L)
#define ULONG_MAX   (4294967295UL)
#endif

#endif /* !_LIMITS_H */

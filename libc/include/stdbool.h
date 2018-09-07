/*******************************************************************************
 * Copyright (c) 2008-2011 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * stdbool.h -- Boolean type and values
 */

#ifndef _STDBOOL_H
#define _STDBOOL_H

#if !defined(__STDC_VERSION__) || (__STDC_VERSION__ < 199901L)
# if !defined(__GNUC__) || (__GNUC__ < 3)
typedef char _Bool;
# endif
#endif

#define bool   _Bool
#define true   1
#define false  0

#define __bool_true_false_are_defined  1

#endif /* !_STDBOOL_H */

/*******************************************************************************
 * Copyright (c) 2008-2011 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * stdio.h
 */

#ifndef _STDIO_H
#define _STDIO_H

#include <compiler.h>
#include <stdarg.h>
#include <stddef.h>

EXTERN int vsnprintf(char *str, size_t size, const char *fmt, va_list ap);
EXTERN int snprintf(char *str, size_t size, const char *fmt, ...);
EXTERN int asprintf(char **strp, const char *fmt, ...);

#endif /* !_STDIO_H */

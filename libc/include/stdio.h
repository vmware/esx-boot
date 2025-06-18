/*******************************************************************************
 * Copyright (c) 2008-2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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

EXTERN int vsnprintf(char *str, size_t size, const char *fmt, va_list ap) __attribute__ ((format (__printf__, 3, 0)));
EXTERN int snprintf(char *str, size_t size, const char *fmt, ...) __attribute__ ((format (__printf__, 3, 4)));
EXTERN int asprintf(char **strp, const char *fmt, ...) __attribute__ ((format (__printf__, 2, 3)));

#endif /* !_STDIO_H */

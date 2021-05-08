/*******************************************************************************
 * Copyright (c) 2008-2011,2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * stdlib.h
 */

#ifndef _STDLIB_H
#define _STDLIB_H

#include <compiler.h>
#include <stddef.h>

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

EXTERN int atoi(const char *);
EXTERN long strtol(const char *, char **, int);
EXTERN unsigned long strtoul(const char *, char **, int);

EXTERN void *malloc(size_t);
EXTERN void *calloc(size_t, size_t);
EXTERN void free(void *);

EXTERN int atexit(void (*func)(void));
EXTERN void do_atexit(void);

#endif /* !_STDLIB_H */

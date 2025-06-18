/*******************************************************************************
 * Copyright (c) 2008-2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * string.h
 */

#ifndef _STRING_H
#define _STRING_H

#include <compiler.h>
#include <stddef.h>

typedef struct {
   const char* key;
   const char* value;
} key_value_t;

EXTERN int strcasecmp(const char *, const char *);
EXTERN int strncasecmp(const char *, const char *, size_t);
EXTERN char *strcat(char *, const char *);
EXTERN char *strchr(const char *, int);
EXTERN char *strrchr(const char *, int);
EXTERN int strcmp(const char *, const char *);
EXTERN char *strcpy(char *, const char *);
EXTERN size_t strlen(const char *);
EXTERN size_t strnlen(const char *, size_t);
EXTERN int strncmp(const char *, const char *, size_t);
EXTERN char *strstr(const char *, const char *);
EXTERN char *strdup(const char *src);

EXTERN void *memchr(const void *, int, size_t);
EXTERN int memcmp(const void *, const void *, size_t);
EXTERN void *memcpy(void *, const void *, size_t);
EXTERN void *memmove(void *, const void *, size_t);
EXTERN void *memset(void *, int, size_t);

#endif /* !_STRING_H */

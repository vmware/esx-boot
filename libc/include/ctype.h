/*******************************************************************************
 * Copyright (c) 2008-2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * ctype.h -- Character classification
 *
 * Warning: This implementation assumes characters are ISO-8859-1, and it
 * doesn't handle EOF, which isn't defined in this package.  For safety, it
 * does handle negative signed characters that have been sign-extended, by
 * masking down to 8 bit width.  The masking is done to avoid having to hunt
 * down and fix other old code in this package that may be sloppy.
 */

#ifndef _CTYPE_H
#define _CTYPE_H

#include <compiler.h>

#define C_CTRL   0
#define C_SPACE  (1 << 0)
#define C_PUNCT  (1 << 1)
#define C_DIGIT  (1 << 2)
#define C_XDIGIT (1 << 3)
#define C_UPPER  (1 << 4)
#define C_LOWER  (1 << 5)
#define C_ALPHA  (C_UPPER | C_LOWER)
#define C_ALNUM  (C_ALPHA | C_DIGIT)
#define C_GRAPH  (C_PUNCT | C_ALNUM)

EXTERN const unsigned char libc_ctype[256];

static INLINE int isspace(int c)
{
   return libc_ctype[c & 0xff] & C_SPACE;
}

static INLINE int isdigit(int c)
{
   return libc_ctype[c & 0xff] & C_DIGIT;
}

static INLINE int isxdigit(int c)
{
   return libc_ctype[c & 0xff] & C_XDIGIT;
}

static INLINE int isupper(int c)
{
   return libc_ctype[c & 0xff] & C_UPPER;
}

static INLINE int islower(int c)
{
   return libc_ctype[c & 0xff] & C_LOWER;
}

static INLINE int isalpha(int c)
{
   return libc_ctype[c & 0xff] & C_ALPHA;
}

static INLINE int ispunct(int c)
{
   return libc_ctype[c & 0xff] & C_PUNCT;
}

static INLINE int isalnum(int c)
{
   return libc_ctype[c & 0xff] & C_ALNUM;
}

static INLINE int isgraph(int c)
{
   return libc_ctype[c & 0xff] & C_GRAPH;
}

static INLINE int isprint(int c)
{
   return c == ' ' || isgraph(c);
}

static INLINE int toupper(int c)
{
   return islower(c) ? (c & ~32) : c;
}

static INLINE int tolower(int c)
{
   return isupper(c) ? (c | 32) : c;
}

#endif /* !_CTYPE_H */

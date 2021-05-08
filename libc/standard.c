/*******************************************************************************
 * Copyright (c) 2008-2011 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * standard.c -- Standard C functions
 */

#include <stddef.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

EXTERN void *sys_malloc(size_t size);
EXTERN void sys_free(void *ptr);

/*-- malloc --------------------------------------------------------------------
 *
 *      Allocate dynamic memory.
 *
 * Parameters
 *      IN size: needed amount of memory, in bytes
 *
 * Results
 *      A pointer to the allocated memory, or NULL if an error occurred.
 *----------------------------------------------------------------------------*/
void *malloc(size_t size)
{
   return sys_malloc(size);
}

/*-- calloc --------------------------------------------------------------------
 *
 *      Allocate dynamic memory for an array of 'nmemb' elements of size 'size'
 *      bytes each. The memory is set to zero.
 *
 * Parameters
 *      IN nmemb: array size, in number of elements
 *      IN size:  element size, in bytes
 *
 * Results
 *      A pointer to the allocated memory, or NULL if an error occurred.
 *----------------------------------------------------------------------------*/
void *calloc(size_t nmemb, size_t size)
{
   void *p;

   size *= nmemb;

   p = malloc(size);
   if (p != NULL) {
      memset(p, 0, size);
   }

   return p;
}

/* free ------------------------------------------------------------------------
 *
 *      Free the memory space pointed to by 'ptr', which must have been returned
 *      by a previous call to malloc() or calloc().
 *
 * Parameters
 *      IN ptr: pointer to the memory to free
 *----------------------------------------------------------------------------*/
void free(void *ptr)
{
   sys_free(ptr);
}

/*-- strtol --------------------------------------------------------------------
 *
 *      Convert the initial part of the string in 'nptr' to a long integer value
 *      according to the given base, which must be between 2 and 36 inclusive,
 *      or be the special value 0.
 *
 *      The string may begin with an arbitrary amount of white space (as
 *      determined by isspace()) followed by a single optional '+' or '-' sign.
 *
 *      If base is 0 or 16, the string may then include a "0x" prefix, and the
 *      number will be read in base 16; otherwise, a zero base is taken as 10
 *      (decimal) unless the next character is '0', in which case it is taken as
 *      8 (octal).
 *
 *      The remainder string is converted to a long int value in the obvious
 *      manner, stopping at the first character which is not a valid digit in
 *      the given base. (In bases above 10, the letter 'A' in either upper or
 *      lower case represents 10, 'B' represents 11 and so forth, with 'Z'
 *      representing 35.)
 *
 * Parameters
 *      IN  nptr:   pointer to the string to convert
 *      OUT endptr: pointer to the first invalid character, or to the original
 *                  input string if there were no digit in at all
 *      IN  base:   the numeric base
 *
 * Results
 *      The result of the conversion, unless the value would underflow or
 *      overflow. If an overflow occurs, strtol() returns LONG_MAX. If an
 *      underflow occurs, strtol() returns LONG_MIN.
 *----------------------------------------------------------------------------*/
long strtol(const char *nptr, char **endptr, int base)
{
   bool negative, overflow;
   const char *end, *s;
   long cutoff, n;
   int cutlim, d;

   for (s = nptr; isspace(*s); s++) {
      ;
   }

   switch (*s) {
      case '-':
         s++;
         negative = true;
         break;
      case '+':
         s++;
         /* Fall through */
      default:
         negative = false;
   }

   if ((base == 0 || base == 16) && s[0] == '0' && (toupper(s[1]) == 'X')) {
      s += 2;
      base = 16;
   } else if (base == 0) {
      base = (s[0] == '0') ? 8 : 10;
   }

   if (negative) {
      cutoff = LONG_MIN / base;
      cutlim = -(LONG_MIN % base);
   } else {
      cutoff = LONG_MAX / base;
      cutlim = LONG_MAX % base;
   }

   overflow = false;
   n = 0;

   for (end = nptr; isalnum(*s); end = ++s) {
      d = isdigit(*s) ? *s - '0' : 10 + toupper(*s) - 'A';
      if (d >= base) {
         break;
      }

      if (!overflow) {
         if (negative && (n < cutoff || (n == cutoff && d > cutlim))) {
            overflow = true;
            n = LONG_MIN;
         } else if (!negative && (n > cutoff || (n == cutoff && d > cutlim))) {
            overflow = true;
            n = LONG_MAX;
         } else {
            n *= base;
            n = negative ? n - d : n + d;
         }
      }
   }

   if (endptr != NULL) {
      *endptr = (char *)end;
   }

   return n;
}

/*-- strtoul -------------------------------------------------------------------
 *
 *      Convert the initial part of the string in 'nptr' to an unsigned long
 *      integer value according to the given base, which must be between 2 and
 *      36 inclusive, or be the special value 0.
 *
 *      The string may begin with an arbitrary amount of white space (as
 *      determined by isspace()) followed by a single optional '+' or '-' sign.
 *
 *      If base is 0 or 16, the string may then include a "0x" prefix, and the
 *      number will be read in base 16; otherwise, a zero base is taken as 10
 *      (decimal) unless the next character is '0', in which case it is taken as
 *      8 (octal).
 *
 *      The remainder string is converted to an unsigned long int value in the
 *      obvious manner, stopping at the first character which is not a valid
 *      digit in the given base. (In bases above 10, the letter 'A' in either
 *      upper or lower case represents 10, 'B' represents 11 and so forth, with
 *      'Z' representing 35.)
 *
 * Parameters
 *      IN  nptr:   pointer to the string to convert
 *      OUT endptr: pointer to the first invalid character, or to the original
 *                  input string if there were no digit in at all
 *      IN  base:   the numeric base
 *
 * Results
 *      The result of the conversion or, if there was a leading minus sign, the
 *      negation of the result of the conversion represented as an unsigned
 *      value,  unless  the  original  (non-negated) value would overflow; in
 *      the latter case, strtoul() returns ULONG_MAX.
 *----------------------------------------------------------------------------*/
unsigned long strtoul(const char *nptr, char **endptr, int base)
{
   bool negative, overflow;
   const char *end, *s;
   unsigned long cutoff, n;
   int cutlim, d;

   for (s = nptr; isspace(*s); s++) {
      ;
   }

   switch (*s) {
      case '-':
         s++;
         negative = true;
         break;
      case '+':
         s++;
         /* Fall through */
      default:
         negative = false;
   }

   if ((base == 0 || base == 16) && s[0] == '0' && (toupper(s[1]) == 'X')) {
      s += 2;
      base = 16;
   } else if (base == 0) {
      base = (s[0] == '0') ? 8 : 10;
   }

   cutoff = ULONG_MAX / base;
   cutlim = ULONG_MAX % base;

   overflow = false;
   n = 0;

   for (end = nptr; isalnum(*s); end = ++s) {
      d = isdigit(*s) ? *s - '0' : 10 + toupper(*s) - 'A';
      if (d >= base) {
         break;
      }

      if (!overflow) {
         if (n > cutoff || (n == cutoff && d > cutlim)) {
            overflow = true;
            n = ULONG_MAX;
         } else {
            n *= base;
            n = n + d;
         }
      }
   }

   if (overflow) {
      n = ULONG_MAX;
   } else if (negative) {
      n = -n;
   }

   if (endptr != NULL) {
      *endptr = (char *)end;
   }

   return n;
}

/*-- atoi ----------------------------------------------------------------------
 *
 *      Convert the initial portion of a string to int.
 *
 * Parameters
 *      IN nptr: pointer to the string
 *
 * Results
 *      The converted value.
 *----------------------------------------------------------------------------*/
int atoi(const char *nptr)
{
    return (int)strtol(nptr, NULL, 10);
}

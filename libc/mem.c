/*******************************************************************************
 * Copyright (c) 2008-2011 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * mem.c -- Operations on memory blocks
 */

#include <sys/types.h>
#include <string.h>

/*-- memmove -------------------------------------------------------------------
 *
 *      Copy a block of memory. The source and the destination may overlap; the
 *      copy is always done in a non-destructive manner.
 *
 * Parameters
 *      IN destination: pointer to the destination buffer
 *      IN source:      pointer to the memory block
 *      IN size:        memory block size, in bytes
 *
 * Results
 *      A pointer to the destination.
 *----------------------------------------------------------------------------*/
void *memmove(void *destination, const void *source, size_t size)
{
   const char *src = source;
   char *dest = destination;

   if (src != dest) {
      if (src < dest) {
         while (size > 0) {
            size--;
            dest[size] = src[size];
         }
      } else {
         while (size > 0) {
            *dest++ = *src++;
            size--;
         }
      }
   }

   return destination;
}

/*-- memset --------------------------------------------------------------------
 *
 *      Fill a memory block with a byte value.
 *
 * Parameters
 *      IN dest: pointer to the memory block
 *      IN c:    byte value
 *      IN n:    memory block size, in bytes
 *
 * Results
 *      A pointer to the byte string.
 *----------------------------------------------------------------------------*/
void *memset(void *dest, int c, size_t n)
{
   unsigned char *p = dest;

   while (n--) {
      *p++ = (unsigned char)c;
   }

   return dest;
}

/*-- memcpy --------------------------------------------------------------------
 *
 *      Copy a block of memory.
 *
 * Parameters
 *      IN dest: pointer to the output buffer
 *      IN src:  pointer to the input buffer
 *      IN n:    number of bytes to copy
 *
 * Results
 *      A pointer to the destination.
 *----------------------------------------------------------------------------*/
void *memcpy(void *dest, const void *src, size_t n)
{
   const char *s = src;
   char *d = dest;

   while (n--) {
      *d++ = *s++;
   }

   return dest;
}

/*-- memcmp --------------------------------------------------------------------
 *
 *      Compare two memory blocks, looking at the first n bytes (each
 *      interpreted as an unsigned char), and returns
 *
 * Parameters
 *       IN s1: pointer to a first memory block
 *       IN s2: pointer to a second memory block
 *       IN n:  number of bytes to compare
 *
 * Results
 *      An integer less than, equal to, or greater than 0, according as s1 is
 *      less than, equal to, or greater than s2 when taken to be unsigned
 *      characters.
 *----------------------------------------------------------------------------*/
int memcmp(const void *s1, const void *s2, size_t n)
{
   const unsigned char *a, *b;

   a = s1;
   b = s2;

   while (n--) {
      if (*a != *b) {
         return (int)*a - (int)*b;
      }
      a++;
      b++;
   }

   return 0;
}

/*-- memchr --------------------------------------------------------------------
 *
 *      Locate the first occurrence of a character in a block of memory. The
 *      character is taken as an int and converted to an unsigned char.
 *
 * Parameters
 *      IN s: pointer to the memory block
 *      IN c: the character to locate
 *      IN n: memory block size, in bytes
 *
 * Results
 *      A pointer to the first occurrence of 'c', or NULL if 'c' does not occur.
 *----------------------------------------------------------------------------*/
void *memchr(const void *s, int c, size_t n)
{
   const unsigned char *p = s;

   while (n--) {
      if (*p == (unsigned char)c) {
         return (void *)p;
      }
      p++;
   }

  return NULL;
}

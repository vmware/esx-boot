/*******************************************************************************
 * Copyright (c) 2008-2011,2013-2016,2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * ucs2.c -- UEFI strings handling routines
 *
 *   UEFI 2.3 Specification says:
 *      "Unless otherwise specified, all characters and strings are stored in
 *      the UCS-2 encoding format as defined by Unicode 2.1 and ISO/IEC 10646
 *      standards."
 */

#include <ctype.h>
#include <string.h>
#include "efi_private.h"

/*-- ucs2_strlen ---------------------------------------------------------------
 *
 *      Return the length of an UCS-2 string.
 *
 * Parameters
 *      IN  Str: pointer to the UCS-2 string
 *
 * Results
 *      The string length (in characters), not including the trailing L'\0'.
 *----------------------------------------------------------------------------*/
size_t ucs2_strlen(const CHAR16 *Str)
{
   size_t len = 0;

   while (Str[len] != L'\0') {
      len++;
   }

   return len;
}

/*-- ucs2_strnlen --------------------------------------------------------------
 *
 *      Return the length of an UCS-2 string that may or may not be
 *      null-terminated.
 *
 * Parameters
 *      IN  Str: pointer to the UCS-2 string
 *
 * Results
 *      The string length (in characters), not including the trailing
 *      L'\0' (if any).
 *----------------------------------------------------------------------------*/
size_t ucs2_strnlen(const CHAR16 *Str, size_t maxlen)
{
   size_t len = 0;

   while (len < maxlen && Str[len] != L'\0') {
      len++;
   }

   return len;
}

/*-- ucs2_strcpy ---------------------------------------------------------------
 *
 *      Copy a UCS-2 string.
 *
 * Parameters
 *      IN  Dest: pointer to the destination buffer
 *      OUT Src:  pointer to the string to copy
 *
 * Results:
 *      A pointer to the destination buffer.
 *----------------------------------------------------------------------------*/
CHAR16 *ucs2_strcpy(CHAR16 *Dest, const CHAR16 *Src)
{
   CHAR16 *Dst = Dest;

   while (*Src != L'\0') {
      *Dst++ = *Src++;
   }
   *Dst = L'\0';

   return Dest;
}

/*-- ucs2_to_ascii -------------------------------------------------------------
 *
 *      UCS-2 to ASCII conversion.
 *
 * Parameters
 *      IN  Src:    pointer to the UCS-2 input string
 *      IN  dest:   pointer to an output buffer that is large enough to hold
 *                  the converted string (trailing '\0' included), or pointer
 *                  to NULL if a new buffer must be allocated
 *      IN  strict: if true and the input string contains a character outside
 *                  the 8-bit ASCII (ISO Latin-1) range, return an error
 *      OUT dest:   pointer to the ASCII output string
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *      EFI_INVALID_PARAMETER if strict and a character is out of range
 *----------------------------------------------------------------------------*/
EFI_STATUS ucs2_to_ascii(const CHAR16 *Src, char **dest, bool strict)
{
   char *str;
   char *p;

   EFI_ASSERT_PARAM(Src != NULL);
   EFI_ASSERT_PARAM(dest != NULL);

   if (*dest == NULL) {
      str = sys_malloc(ucs2_strlen(Src) + 1);
      if (str == NULL) {
         return EFI_OUT_OF_RESOURCES;
      }
   } else {
      str = *dest;
   }

   p = str;
   do {
      if (strict && ((*Src) & 0xff00) != 0) {
         if (*dest == NULL) {
            sys_free(str);
         }
         return EFI_INVALID_PARAMETER;
      }
      *p++ = (char)(*Src);
   } while (*Src++ != L'\0');

   *dest = str;
   return EFI_SUCCESS;
}

/*-- ascii_to_ucs2 -------------------------------------------------------------
 *
 *      Convert an ASCII string into UCS-2. In the case where a destination
 *      buffer is provided, source and destination buffers may overlap
 *
 * Parameters
 *      IN  src:  pointer to the ASCII input string
 *      IN  Dest: pointer to an output buffer that is large enough to hold the
 *                converted string (trailing '\0' included), or pointer to NULL
 *                if a new buffer must be allocated
 *      OUT Dest: pointer to the UCS-2 output string
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS ascii_to_ucs2(const char *src, CHAR16 **Dest)
{
   CHAR16 *Str;
   EFI_STATUS Status;
   char *p;
   size_t len;

   EFI_ASSERT_PARAM(src != NULL);
   EFI_ASSERT_PARAM(Dest != NULL);

   len = strlen(src);

   if (*Dest == NULL) {
      Status = ucs2_alloc(len, Dest);
      if (EFI_ERROR(Status)) {
         return Status;
      }
   } else {
      len++;
      p = (char *)*Dest + len * (sizeof (CHAR16) - sizeof (char));
      memmove(p, src, len);
      src = p;
   }

   Str = *Dest;
   do {
      *Str++ = (CHAR16)(*src);
   } while (*src++ != '\0');

   return EFI_SUCCESS;
}

/*-- ucs2_alloc ----------------------------------------------------------------
 *
 *      Allocate space for a new UCS-2 string. The first character of the string
 *      is initialized with the '\0' delimiter.
 *
 * Parameters
 *      IN  length: number of characters in the string (not including the
 *                  trailing '\0')
 *      OUT Str:    pointer to the freshly allocated string
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS ucs2_alloc(size_t length, CHAR16 **Str)
{
   CHAR16 *p;

   p = sys_malloc((length + 1) * sizeof (CHAR16));
   if (p == NULL) {
      return EFI_OUT_OF_RESOURCES;
   }

   p[0] = L'\0';
   *Str = p;

   return EFI_SUCCESS;
}

/*-- ucs2_strdup ---------------------------------------------------------------
 *
 *      Dupicate a UCS-2 string.
 *
 * Parameters
 *      IN  Str:        Pointer to the string to duplicate
 *      OUT Duplicate:  pointer to the duplicate
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS ucs2_strdup(const CHAR16 *Str, CHAR16 **Duplicate)
{
   EFI_STATUS Status;

   Status = ucs2_alloc(ucs2_strlen(Str), Duplicate);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   ucs2_strcpy(*Duplicate, Str);

   return EFI_SUCCESS;
}

/*-- ucs2_toupper --------------------------------------------------------------
 *
 *      Convert a UCS-2 character to upper case.
 *
 * Parameters
 *      IN C: input character
 *
 * Results
 *      the upper case character equivalent
 *----------------------------------------------------------------------------*/
CHAR16 ucs2_toupper(CHAR16 C)
{
   if (C < 256) {
      return (CHAR16)toupper(C);
   }

   return C;
}

/*-- ucs2_strcmp- --------------------------------------------------------------
 *
 *      Compare two UCS-2 strings to each other.
 *
 * Parameters
 *      IN Str1: pointer to a first string
 *      IN Str2: pointer to a second string
 *
 * Results
 *      An integer less than, equal to, or greater than zero if Str1 is found,
 *      respectively, to be less than, to match, or be greater than Str2.
 *----------------------------------------------------------------------------*/
int ucs2_strcmp(const CHAR16 *Str1, const CHAR16 *Str2)
{
   while (1) {
      if (*Str1 == L'\0' || *Str1 != *Str2) {
         break;
      }
      Str1++;
      Str2++;
   }

   return (int)*Str1 - (int)*Str2;
}

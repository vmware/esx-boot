/*******************************************************************************
 * Copyright (c) 2008-2015 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * string.c -- Operations on strings
 */

#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

/*-- strcpy --------------------------------------------------------------------
 *
 *      Copy a string (including the terminating '\0' character). Source and
 *      destination strings may not overlap, and the destination string must be
 *      large enough to receive the copy.
 *
 * Parameters
 *      IN dest: pointer to the output buffer
 *      IN src:  pointer to the input string
 *
 * Results
 *      A pointer to the copy
 *----------------------------------------------------------------------------*/
char *strcpy(char *dest, const char *src)
{
   size_t i;

   for (i = 0; src[i] != '\0'; i++) {
      dest[i] = src[i];
   }

   dest[i] = '\0';

   return dest;
}

/*-- strcat --------------------------------------------------------------------
 *
 *      Append a string (including the trailing '\0') to the end of another.
 *
 * Parameters
 *      IN dest: pointer to the string to be appended
 *      IN src:  pointer to the string to append
 *
 * Results
 *      A pointer to the destination string.
 *----------------------------------------------------------------------------*/
char *strcat(char *dest, const char *src)
{
   size_t len;

   len = strlen(dest);
   strcpy(dest + len, src);

   return dest;
}

/*-- strchr --------------------------------------------------------------------
 *
 *      Locate the first occurrence of a character in a string. The character is
 *      taken as an int and converted to a char. The trailing '\0' is considered
 *      to be part of the string.
 *
 * Parameters
 *      IN s: pointer to the string to search in
 *      IN c: the character to locate
 *
 * Results
 *      A pointer to the first occurrence of 'c', or NULL if 'c' does not occur.
 *----------------------------------------------------------------------------*/
char *strchr(const char *s, int c)
{
   do {
      if (*s == (char)c) {
         return (char *)s;
      }
   } while (*s++ != '\0');

   return NULL;
}

/*-- strrchr -------------------------------------------------------------------
 *
 *      Locate the last occurrence of a character in a string. The character is
 *      taken as an int and converted to a char. The trailing '\0' is considered
 *      to be part of the string.
 *
 * Parameters
 *      IN s: pointer to the string to search in
 *      IN c: the character to locate
 *
 * Results
 *      A pointer to the last occurrence of 'c', or NULL if 'c' does not occur.
 *----------------------------------------------------------------------------*/
char *strrchr(const char *s, int c)
{
   const char *t = s + strlen(s);
   do {
      if (*t == (char)c) {
         return (char *)t;
      }
   } while (--t >= s);

   return NULL;
}

/*-- strstr --------------------------------------------------------------------
 *
 *      locates the first occurrence of a string (excluding the terminating null
 *      character) within another string. Try to locate the empty string (the
 *      string ""), returns a pointer to 'needle'.
 *
 * Parameters
 *      IN haystack: pointer to the string to locate
 *      IN needle:   pointer to the string to search into
 *
 * Results
 *      A pointer to the located string, or NULL if the string is not found.
 *----------------------------------------------------------------------------*/
char *strstr(const char *haystack, const char *needle)
{
   size_t i, len1, len2;

   len1 = strlen(haystack);
   len2 = strlen(needle);

   if (len1 >= len2) {
      for (i = 0; i <= len1 - len2; i++) {
         if (strncmp(&haystack[i], needle, len2) == 0) {
            return (char *)&haystack[i];
         }
      }
   }

   return NULL;
}

/*-- strlen --------------------------------------------------------------------
 *
 *      Return the length of a string, in characters, not including the trailing
 *      '\0'.
 *
 * Parameters
 *      IN s: pointer to the string
 *
 * Results
 *      The length, in number of characters
 *----------------------------------------------------------------------------*/
size_t strlen(const char *s)
{
   size_t i = 0;

   while (s[i] != '\0') {
      i++;
   }

   return i;
}

/*-- strnlen -------------------------------------------------------------------
 *
 *      Return the length of a string, in characters, not including the trailing
 *      '\0', but at most maxlen.
 *
 * Parameters
 *      IN s: pointer to the string
 *
 * Results
 *      The length, in number of characters, up to maxlen.
 *----------------------------------------------------------------------------*/
size_t strnlen(const char *s, size_t maxlen)
{
   size_t i = 0;

   while (i < maxlen && s[i] != '\0') {
      i++;
   }

   return i;
}

/*-- strcmp --------------------------------------------------------------------
 *
 *      Compare two strings to each other.
 *
 * Parameters
 *      IN s1: pointer to a first string
 *      IN s2: pointer to a second string
 *
 * Results
 *      An integer less than, equal to, or greater than zero if s1 is found,
 *      respectively, to be less than, to match, or be greater than s2.
 *----------------------------------------------------------------------------*/
int strcmp(const char *s1, const char *s2)
{
   while (1) {
      if (*s1 == '\0' || *s1 != *s2) {
         break;
      }
      s1++;
      s2++;
   }

   return (int)*s1 - (int)*s2;
}

/*-- strncmp -------------------------------------------------------------------
 *
 *      Compare the first (at most) n bytes of two strings.
 *
 * Parameters
 *      IN s1: pointer to a first string
 *      IN s2: pointer to a second string
 *      IN n:  maximum number of bytes to compare
 *
 * Results
 *      An integer less than, equal to, or greater than zero if s1 is found,
 *      respectively, to be less than, to match, or be greater than s2.
 *----------------------------------------------------------------------------*/
int strncmp(const char *s1, const char *s2, size_t n)
{
   while (n--) {
      if (*s1 == '\0' || *s1 != *s2) {
         return (int)*s1 - (int)*s2;
      }
      s1++;
      s2++;
   }

   return 0;
}

/*-- strcasecmp ---------------------------------------------------------------
 *
 *      Compare two strings in a case-insensitive manner (i.e. ignore
 *      differences in case when comparing lower and upper case characters).
 *
 * Parameters
 *      IN s1: pointer to a first string
 *      IN s2: pointer to a second string
 *
 * Results
 *      An integer less than, equal to, or greater than zero if s1 is found,
 *      respectively, to be less than, to match, or be greater than s2.
 *----------------------------------------------------------------------------*/
int strcasecmp(const char *s1, const char *s2)
{
   while (*s1 != '\0' && *s2 != '\0' && toupper(*s1) == toupper(*s2)) {
      s1++;
      s2++;
   }

   return (int)*s1 - (int)*s2;
}

/*-- strncasecmp ---------------------------------------------------------------
 *
 *      Compare the first (at most) n bytes of two strings in a case-insensitive
 *      manner (i.e. ignore differences in case when comparing lower and upper
 *      case characters).
 *
 * Parameters
 *      IN s1: pointer to a first string
 *      IN s2: pointer to a second string
 *      IN n:  maximum number of bytes to compare
 *
 * Results
 *      An integer less than, equal to, or greater than zero if s1 is found,
 *      respectively, to be less than, to match, or be greater than s2.
 *----------------------------------------------------------------------------*/
int strncasecmp(const char *s1, const char *s2, size_t n)
{
   while (n--) {
      if (*s1 == '\0' || toupper(*s1) != toupper(*s2)) {
         return (int)*s1 - (int)*s2;
      }
      s1++;
      s2++;
   }

   return 0;
}

/*-- strdup --------------------------------------------------------------------
 *
 *      Duplicate a string.
 *
 * Parameters
 *      IN src: pointer to the string to duplicate
 *
 * Results
 *      A pointer to the duplicated string, or NULL if an error occurred.
 *----------------------------------------------------------------------------*/
char *strdup(const char *src)
{
   char *dest;

   dest = malloc(strlen(src) + 1);
   if (dest == NULL) {
      return NULL;
   }

   return strcpy(dest, src);
}

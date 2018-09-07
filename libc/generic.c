/*******************************************************************************
 * Copyright (c) 2008-2013,2015 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * generic.c -- Pattern matching
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/*-- dirname -------------------------------------------------------------------
 *
 *      Returns the string up to, but not including, the '/' delimiter. If path
 *      does not contain a slash, dirname() returns ".". If path is a NULL
 *      pointer or points to an empty string, dirname() returns the string "."
 *
 * Parameters
 *      IN path: pointer to the pathname
 *
 * Results
 *      A pointer to the output directory path.
 *
 * Side Effects
 *      Input pathname is modified in place.
 *----------------------------------------------------------------------------*/
char *dirname(char *path)
{
   static char slashdot[4] = {'/', '\0', '.', '\0'};
   bool slash;
   size_t len;
   char *p;

   /* Return "." for NULL or empty strings */
   if (path == NULL || (*path == '\0')) {
      return &slashdot[2];
   }

   /* Remove trailing slashes */
   p = path + strlen(path) - 1;
   while (p > path && *p == '/') {
      p--;
   }
   /* Discard the basename */
   while (p > path && *p != '/') {
      p--;
   }

   if (p == path) {
      /* Either the dir is "/" or there are no slashes */
      return &slashdot[(*p == '/') ? 0 : 2];
   }
   *p = '\0';

   /* Merge repetitive slashes */
   slash = false;
   len = 0;
   for (p = path; *p != '\0'; p++) {
      if (*p == '/') {
         if (slash) {
            continue;
         }
         slash = true;
      } else {
         if (slash) {
            slash = false;
         }
      }
      path[len++] = *p;
   }

   /* Remove final '/' if any */
   if (len > 1 && path[len - 1] == '/') {
      len--;
   }
   path[len] = '\0';

   return path;
}

/*-- basename ----------------------------------------------------------------
 *
 *      Returns the component of the path following the last '/'. Trailing '/'
 *      characters are not considered part of the pathname.
 *      If path does not contain a '/' then basename returns a copy of the
 *      path. If path is a NULL pointer or points to an empty string,
 *      basename() returns the string "."
 *
 * Parameters
 *      IN path: Pointer to the path of the given file.
 *
 * Results
 *      A pointer to the filename.
 *
 * Side Effects
 *      Input pathname may be modified in place if necessary, to remove
 *      trailing '/' characters.
 *----------------------------------------------------------------------------*/
char *basename(char *path)
{
   static char slashdot[4] = {'/', '\0', '.', '\0'};
   char *p;

   /* Return "." for NULL or empty strings */
   if (path == NULL || (*path == '\0')) {
      return &slashdot[2];
   }

   /* Remove trailing slashes */
   p = path + strlen(path) - 1;
   while (p > path && *p == '/') {
      *p = '\0';
      p--;
   }

   if (p == path) {
      /* Either the path is "/" or there are no slashes */
      return (*p == '/' ? &slashdot[0] : path);
   }

   p = path + strlen(path) - 1;
   while (p > path && *p != '/') {
      p--;
   }

   return (*p == '/' ? (p + 1) : p);
}

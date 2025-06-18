/*******************************************************************************
 * Copyright (c) 2008-2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * string.c -- Operations on strings
 */

#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <boot_services.h>
#include <bootlib.h>

/*-- str_alloc ----------------------------------------------------------------
 *
 *      Allocate space for a new string. The first character of the string is
 *      initialized with the '\0' delimiter.
 *
 * Parameters
 *      IN  length: number of characters in the string (not including the
 *                  trailing '\0')
 *      OUT str:    pointer to the freshly allocated string
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int str_alloc(size_t length, char **str)
{
   char *s;

   s = malloc(length + 1);
   if (s == NULL) {
      return ERR_OUT_OF_RESOURCES;
   }

   *s = '\0';
   *str = s;

   return ERR_SUCCESS;
}

/*-- mem_swap ------------------------------------------------------------------
 *
 *      Swap memory contents pointed by p1 and p2.
 *
 * Parameters
 *      IN p1: pointer to a memory location
 *      IN p2: pointer to another memory location
 *      IN n:  number of bytes to swap
 *----------------------------------------------------------------------------*/
void mem_swap(void *p1, void *p2, size_t n)
{
   char *p = p1;
   char *q = p2;
   char tmp;

   for ( ; n > 0; n--, p++, q++) {
      tmp = *p;
      *p = *q;
      *q = tmp;
   }
}

/*-- mem_strcasestr ------------------------------------------------------------
 *
 *      Locate a string within a memory region. The '\0' string delimiter is not
 *      considered to be part of the string.
 *
 * Parameters
 *      IN src: pointer to the memory region
 *      IN str: pointer to the string to search
 *      IN n:   memory region size, in bytes
 *
 * Results
 *      A pointer to located string, or NULL if the string was not found.
 *----------------------------------------------------------------------------*/
char *mem_strcasestr(const void *src, const char *str, size_t n)
{
   size_t len;

   len = strlen(str);

   if (n >= len) {
      n -= len;

      do {
         if (strncasecmp(src, str, len) == 0) {
            return (char *)src;
         }
         src = (void *)((char *)src + 1);
      } while (n--);
   }

   return NULL;
}

/*-- str_merge_spaces ----------------------------------------------------------
 *
 *      - Delete front and trailing whitespaces
 *      - Replace any sequence of whitespaces (see isspace()) with a single
 *        space character.
 *
 * Parameters
 *      IN str: pointer to the input string
 *
 * Results
 *      A pointer to the formated input string.
 *
 * Side Effects
 *     The input string is directly modified.
 *----------------------------------------------------------------------------*/
char *str_merge_spaces(char *str)
{
   int i, j;
   bool space = true;

   for (i = 0, j = 0; str[i] != '\0'; i++) {
      if (isspace(str[i])) {
         if (!space) {
            str[j++] = ' ';
            space = true;
         }
      } else {
         str[j++] = str[i];
         space = false;
      }
   }

   if (j > 0 && str[j - 1] == ' ') {
      j--;
   }

   str[j] = '\0';

   return str;
}

/*-- cmdline_split -------------------------------------------------------------
 *
 *      Split the command line by replacing space sequences by the '\0'
 *      delimiter. Quoted spaces are ignored.
 *
 * Parameters
 *      IN  str:  the C-string formatted command line
 *      OUT argc: number of arguments in the command line
 *      IN  amp:  if true, treat '&' as a space as well
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int cmdline_split(char *str, int *argc, bool amp)
{
   bool onseparator;
   char quote;
   char *p;
   int n;

   onseparator = true;
   quote = '\0';
   n = 0;
   p = str;

   for ( ; *str != '\0'; str++) {
      if (*str == '\'' || *str == '\"') {
         if (quote == '\0') {
            quote = *str;
            continue;
         } else if (quote == *str) {
            quote = '\0';
            continue;
         }
      }

      if ((isspace(*str) || (amp && *str == '&')) && quote == '\0') {
         if (!onseparator) {
            *p++ = '\0';
            onseparator = true;
         }
      } else if (onseparator) {
         *p++ = *str;
         n++;
         onseparator = false;
      } else {
         *p++ = *str;
      }
   }

   while (p < str) {
      *p++ = '\0';
   }

   if (quote != '\0') {
      return ERR_SYNTAX;
   }

   *argc = n;

   return ERR_SUCCESS;
}

/*-- str_to_argv ---------------------------------------------------------------
 *
 *      Convert a string representing a command line to an argv[] like array.
 *      The argv[] array is internally allocated with malloc() and should
 *      be freed with free() when no longer used.  The cmdline is modified
 *      in place and the argv array retains pointers into it, so the cmdline
 *      must not be freed until the argv array is no longer in use.
 *
 * Parameters
 *      IN  cmdline: the C-string formatted command line
 *      OUT argc:    number of arguments in the command line
 *      OUT argv:    pointer to the arguments array
 *      IN  amp:     if true, treat '&' as an argument separator
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *
 * Side effects
 *      The command line is modified in place and pointers into it are retained.
 *----------------------------------------------------------------------------*/
int str_to_argv(char *cmdline, int *argc, char ***argv, bool amp)
{
   char **args;
   int n, status;

   status = cmdline_split(cmdline, &n, amp);
   if (status != ERR_SUCCESS) {
      return status;
   }

   if (n > 0) {
      args = malloc(n * sizeof (char *));
      if (args == NULL) {
         return ERR_OUT_OF_RESOURCES;
      }

      *argc = n;
      *argv = args;

      for (n = 0; n < *argc; n++) {
         args[n] = cmdline;
         cmdline += STRSIZE(cmdline);
      }
   } else {
      *argc = 0;
      *argv = NULL;
   }

   return ERR_SUCCESS;
}

/*-- argv_to_str ---------------------------------------------------------------
 *
 *      Convert an argv-like array to a single string. The string is allocated
 *      via malloc().
 *
 * Parameters
 *      IN  argc: number of arguments on the command line
 *      IN  argv: pointer to the command line arguments list
 *      OUT s:    pointer to the freshly allocated output string
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int argv_to_str(int argc, char **argv, char **s)
{
   size_t size;
   char *str;
   int i, status;

   if ((argc > 0 && argv == NULL) || (s == NULL)) {
      return ERR_INVALID_PARAMETER;
   }

   size = 0;
   for (i = 0; i < argc; i++) {
      size += strlen(argv[i]);
      if (i > 0) {
         size++;
      }
   }

   status = str_alloc(size, &str);
   if (status != ERR_SUCCESS) {
      return status;
   }

   for (i = 0; i < argc; i++) {
      if (i > 0) {
         strcat(str, " ");
      }
      strcat(str, argv[i]);
   }

   *s = str;

   return ERR_SUCCESS;
}

/*-- file_sanitize_path --------------------------------------------------------
 *
 *      Sanitize a UNIX-style or URL-style path
 *        - the first :// sequence, if any, keeps its //
 *        - other multiple-separator / occurrences are merged
 *        - whitespace characters are removed (XXX why?)
 *
 * Parameters
 *      IN  filepath: pointer to the UNIX/Protocol path
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *
 * Side Effects
 *      Input pathname is modified in place
 *----------------------------------------------------------------------------*/
int file_sanitize_path(char *filepath)
{
   const char *p;
   bool slash;
   int i, j;

   if (filepath == NULL) {
      return ERR_INVALID_PARAMETER;
   }

   slash = false;
   p = strstr(filepath, "://");
   j = 0;

   for (i = 0; filepath[i] != '\0'; i++) {
      if (isspace(filepath[i])) {
         continue;
      } else if (filepath[i] == '/' || filepath[i] == '\\') {
         if (!slash || ((p != NULL) && (&filepath[i] < (p + 3)))) {
            filepath[j++] = '/';
            slash = true;
         }
      } else {
         filepath[j++] = filepath[i];
         slash = false;
      }
   }

   filepath[j] = '\0';

   return ERR_SUCCESS;
}

/*-- make_path -----------------------------------------------------------------
 *
 *      Concatenate the path of a default root directory with a relative file
 *      path. The given file path is duplicated if it is actually already
 *      absolute.
 *
 *      URLs (containing "://") are treated as absolute paths.
 *
 * Parameters
 *      IN  root:     the root directory absolute path
 *      IN  filepath: the relative file path
 *      OUT buffer:   pointer to the freshly allocated absolute file path
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int make_path(const char *default_root_dir, const char *filepath, char **buffer)
{
   char *path;
   int status;

   if (filepath == NULL || buffer == NULL) {
      return ERR_INVALID_PARAMETER;
   }

   while (isspace(*filepath)) {
      filepath++;
   }

   if (is_absolute(filepath)) {
      default_root_dir = NULL;
   } else {
      if (default_root_dir == NULL) {
         return ERR_INVALID_PARAMETER;
      }
      while (isspace(*default_root_dir)) {
         default_root_dir++;
      }
   }

   if (default_root_dir != NULL && default_root_dir[0] != '\0') {
      if (asprintf(&path, "%s/%s", default_root_dir, filepath) == -1) {
         path = NULL;
      }
   } else {
      path = strdup(filepath);
   }

   if (path == NULL) {
      return ERR_OUT_OF_RESOURCES;
   }

   status = file_sanitize_path(path);
   if (status != ERR_SUCCESS) {
      free(path);
      return status;
   }

   *buffer = path;

   return ERR_SUCCESS;
}

/*-- is_number -----------------------------------------------------------------
 *
 *      Check whether a string consists of an optional leading minus sign
 *      followed only by digit characters.
 *
 * Parameters
 *      IN str: input string
 *
 * Results
 *      true if the string represents a number, false otherwise.
 *----------------------------------------------------------------------------*/
bool is_number(const char *str)
{
   if (*str == '-') {
      str++;
   }
   while (*str != '\0') {
      if (!isdigit((int)*str++)) {
         return false;
      }
   }

   return true;
}

/*-- insert_char ---------------------------------------------------------------
 *
 *      Insert a character before the n-th character in the specified string.
 *      The buffer containing the string must be large enough to hold the result
 *      string.
 *
 * Parameters
 *      IN strbuf: pointer to the string buffer
 *      IN c:      the character to insert
 *      IN offset: insertion offset
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int insert_char(char *strbuf, char c, size_t offset)
{
   size_t i;

   if (strbuf == NULL || offset > strlen(strbuf)) {
      return ERR_INVALID_PARAMETER;
   }

   for (i = STRSIZE(strbuf); i >= offset + 1; i--) {
      strbuf[i] = strbuf[i - 1];
   }

   strbuf[offset] = c;

   return ERR_SUCCESS;
}

/*-- delete_char ---------------------------------------------------------------
 *
 *      Delete the n-th character in a string.
 *
 * Parameters
 *      IN str: pointer to the string
 *      IN n:   index of the character to delete
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int delete_char(char *str, size_t n)
{
   size_t len;

   if (str == NULL || n >= strlen(str)) {
      return ERR_INVALID_PARAMETER;
   }

   len = strlen(str);

   for ( ; n < len; n++) {
      str[n] = str[n + 1];
   }

   return ERR_SUCCESS;
}

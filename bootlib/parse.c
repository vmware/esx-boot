/*******************************************************************************
 * Copyright (c) 2008-2013 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * parse.c -- Configuration file parsing
 *
 *      Parse a configuration file made of Key/Value options.
 *      The parsing is processed line by line. Each line represents a couple
 *      (key, value) which defines for each option, its name and its setting.
 *
 *      General syntax:                |   Example (separator is "="):
 *                                     |
 *           # COMMENT                 |        # Boot configuration file
 *           KEY1 SEPARATOR VALUE1     |        title   = "Welcome!!"
 *           KEY2 SEPARATOR VALUE2     |        kernel  = /boot/kernel.gz
 *                    ...              |        dhcp    = FALSE
 *           KEYn SEPARATOR VALUEn     |        timeout = 15
 *
 *      Parsing rules
 *        - Spaces at the beginning and end of each line are ignored
 *        - Spaces around the separator are ignored.
 *        - lines which begin with a '#' character are comments
 */

#include <string.h>
#include <stdlib.h>
#include <boot_services.h>
#include <bootlib.h>

/*-- locate_value --------------------------------------------------------------
 *
 *      This function tries to locate the given Key and Separator. If both are
 *      found, it returns a pointer to the Value within the input string.
 *
 * Parameters
 *      IN  line:      pointer to the input string to parse
 *      IN  key:       pointer to the key string
 *      IN  separator: pointer to the separator string
 *
 * Results
 *      A pointer to the value within the input string, or NULL if the value
 *      is not found.
 *----------------------------------------------------------------------------*/
static char *locate_value(const char *line, const char *key,
                          const char *separator)
{
   char *sep_start, *sep_end;

   sep_start = strstr(line, separator);
   if (sep_start == NULL) {
      /* line does not contain a valid separator. */
      return NULL;
   }
   sep_end = sep_start + strlen(separator);

   /* Ignore spaces between the keyword and the separator. */
   while (sep_start > line && sep_start[0] == ' ') {
      sep_start--;
   }

   if (strncasecmp(key, line, sep_start - line) != 0) {
      return NULL;
   }

   /* Ignore spaces between the separator and the value. */
   while (sep_end[0] == ' ') {
      sep_end++;
   }

   return sep_end;
}

/*-- parse_option --------------------------------------------------------------
 *
 *      Parse a Key/Value option line.
 *
 * Parameters
 *      IN line:    pointer to the input line
 *      IN options: pointer to the option dispatch structure
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int parse_option(char *line, option_t *options)
{
   char *value;

   if (line == NULL) {
      return ERR_INVALID_PARAMETER;
   }

   str_merge_spaces(line);
   if (line[0] == '\0' || line[0] == '#') {
      /* Line is either empty or commented: do nothing */
      return ERR_SUCCESS;
   }

   for ( ; options->key != NULL; options++) {
      value = locate_value(line, options->key, options->separator);

      if (value != NULL) {
         if (value[0] == '\0') {
            options->value.integer = 0;
            return ERR_SUCCESS;
         }

         switch (options->type) {
            case OPT_STRING:
               options->value.str = strdup(value);
               if (options->value.str == NULL) {
                  return ERR_OUT_OF_RESOURCES;
               }
               return ERR_SUCCESS;
            case OPT_INTEGER:
               if (!is_number(value)) {
                  return ERR_SYNTAX;
               }
               options->value.integer = atoi(value);
               return ERR_SUCCESS;
            default:
               /* We should never be here */
               return ERR_INVALID_PARAMETER;
         }
      }
   }

   /* Ignore unknown keys */
   return ERR_SUCCESS;
}

/*-- parse_config_file ---------------------------------------------------------
 *
 *      Parse a key/value based configuration file. Valid (key/value) couples
 *      are described via the opts parameter. The configuration file is loaded
 *      into memory before being processed line by line.
 *
 * Parameters
 *      IN volid:    MBR/GPT partition number
 *      IN filename: pointer to the configuration file filename
 *      IN options:  pointer to the option dispatch structure
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int parse_config_file(int volid, const char *filename, option_t *options)
{
   char *p, *newline, *line;
   void *buffer;
   size_t size, len;
   int status;
   option_t *opt;

   status = file_load(volid, filename, NULL, &buffer, &size);
   if (status != ERR_SUCCESS) {
      return status;
   }

   for (opt = options; opt->type != OPT_INVAL; opt++) {
      switch (opt->type) {
      case OPT_STRING:
         opt->value.str = NULL;
         break;
      case OPT_INTEGER:
         opt->value.integer = 0;
         break;
      case OPT_INVAL:
         // not reached
         break;
      }
   }

   status = 0;
   p = buffer;
   while (size > 0) {
      newline = memchr(p, '\n', size);
      len = (newline != NULL) ? (size_t)(newline - p) : size;

      if (len > 0) {
         line = sys_malloc(len + 1);
         if (line == NULL) {
            sys_free(buffer);
            return ERR_OUT_OF_RESOURCES;
         }

         memcpy(line, p, len);
         line[len] = '\0';

         status = parse_option(line, options);
         sys_free(line);
         if (status != 0) {
            break;
         }
      }

      if (newline != NULL) {
         len++;
      }
      size -= len;
      p += len;
   }

   sys_free(buffer);
   return status;
}

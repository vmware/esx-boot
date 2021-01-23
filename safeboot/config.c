/*******************************************************************************
 * Copyright (c) 2008-2013,2019-2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * config.c -- Safeboot configuration file parsing
 */

#include <string.h>
#include <ctype.h>
#include <boot_services.h>

#include "safeboot.h"

/*
 * safeboot configuration file options.
 */
static option_t opts[] = {
   {"build", "=", {NULL}, OPT_STRING, {0}},
   {"updated", "=", {.integer = 0}, OPT_INTEGER, {0}},
   {"bootstate", "=", {.integer = 0}, OPT_INTEGER, {0}},
   {"quickboot", "=", {.integer = 0}, OPT_INTEGER, {0}},
   {NULL, NULL, {NULL}, OPT_INVAL, {0}}
};

/*-- bank_get_config -----------------------------------------------------------
 *
 *      Parse the given configuration file to fill up the bootbank information
 *      structure.
 *
 * Parameters
 *      IN bank: pointer to the boot banks info structure
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int bank_get_config(bootbank_t *bank)
{
   int status;

   ASSERT(bank != NULL);

   status = parse_config_file(bank->volid, SAFEBOOT_CFG, opts);
   if (status != ERR_SUCCESS) {
      return status;
   }

   bank->build = opts[0].value.str;
   bank->updated = opts[1].value.integer;
   bank->bootstate = opts[2].value.integer;
   bank->quickboot = opts[3].value.integer;

   return ERR_SUCCESS;
}

/*-- scan_config ---------------------------------------------------------------
 *
 *      Scan the given buffer for the given keyword followed by the associated
 *      separator. If a substring matches at the beginning of a line (white
 *      spaces are ignored), then this function returns a pointer to the next
 *      non-blank character after the separator.
 *
 *      Note: This function is case insensitive.
 *
 * Parameters
 *      IN buffer:    pointer to the bootbank configuration buffer
 *      IN buflen:    size of the bootbank configuration buffer
 *      IN keyword:   pointer to the keyword string
 *      IN separator: pointer to the separator string
 *
 * Results
 *      A pointer within the configuration buffer to the value string associated
 *      with the given (keyword, separator), or NULL if not found.
 *----------------------------------------------------------------------------*/
char *scan_config(const char *buffer, size_t buflen, const char *keyword,
                  const char *separator)
{
   const char *p, *tmp, *limit;
   size_t len;
   bool found;

   if (buflen == 0) {
      return NULL;
   }

   p = buffer;
   limit = buffer + buflen;

   do {
      p = mem_strcasestr(p, keyword, buflen - (p - buffer));
      if (p == NULL) {
         return NULL;
      }

      found = true;

      if (p > buffer) {
         /*
          * Look backward to make sure we are the first word of a line.
          * This avoids matching the keyword in the kernel options, or in a
          * comment.
          */
         for (tmp = p - 1; tmp >= buffer && *tmp != '\n'; tmp--) {
            if (!isspace(*tmp)) {
               found = false;
               break;
            }
         }
      }

      p += strlen(keyword);
      if (found) {
         /*
          * Now look forward to see if "bootstate" is followed by optional
          * spaces, and a valid separator.
          */
         found = false;
         len = strlen(separator);
         for ( ; p < limit; p++) {
            if (*p != ' ' && *p != '\t') {
               if (strncasecmp(p, separator,
                               MIN(len, (size_t)(limit - p))) == 0) {
                  p += len;
                  found = true;
               }
               break;
            }
         }
      }
   } while (!found);

   for ( ; p < limit; p++) {
      if (*p != ' ' && *p != '\t') {
         return (char *)p;
      }
   }

   return NULL;
}

/*-- bank_set_bootstate --------------------------------------------------------
 *
 *      Update the bootstate in the configuration file. the bootstate must be
 *      located within the first 512 bytes of the configuration file.
 *
 * Parameters
 *      IN bank:      pointer to the boot banks info structure
 *      IN bootstate: the new boot state value
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int bank_set_bootstate(bootbank_t *bank, int bootstate)
{
   char *buffer, *state_ptr;
   size_t size;
   int status;

   if (bank == NULL ||
       (bootstate != BANK_STATE_DIRTY && bootstate != BANK_STATE_INVALID)) {
      return ERR_INVALID_PARAMETER;
   }

   status = file_load(bank->volid, SAFEBOOT_CFG, NULL, (void **)&buffer, &size);
   if (status != ERR_SUCCESS) {
      return status;
   }

   /* Our BIOS utils library is not able to write a file beyond 512 bytes. */
   size = MIN(size, 512);

   state_ptr = scan_config(buffer, size, "bootstate", "=");
   if (state_ptr == NULL) {
      Log(LOG_ERR, "BANK%d: boot state not found.\n", bank->volid);
      sys_free(buffer);
      return ERR_SYNTAX;
   }

   if (*state_ptr < '0' || *state_ptr > '3') {
      sys_free(buffer);
      Log(LOG_ERR, "BANK%d: invalid boot state.\n", bank->volid);
      return ERR_SYNTAX;
   }

   Log(LOG_DEBUG, "BANK%d: updating boot state from %s to %s.\n", bank->volid,
       bootstate_to_str(*state_ptr - '0'), bootstate_to_str(bootstate));

   *state_ptr = '0' + bootstate;

   if (safeboot.fake_write_err) {
      status = ERR_UNKNOWN;
   } else {
      status = file_overwrite(bank->volid, SAFEBOOT_CFG, buffer, size);
   }
   sys_free(buffer);

   if (status != ERR_SUCCESS) {
      Log(LOG_WARNING, "BANK%d: failed to overwrite %s: %d (ignored)\n",
          bank->volid, SAFEBOOT_CFG, status);
   }

   bank->bootstate = bootstate;
   return ERR_SUCCESS;
}

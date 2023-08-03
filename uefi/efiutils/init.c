/*******************************************************************************
 * Copyright (c) 2008-2022 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * init.c -- EFI Firmware init/cleanup functions
 */

#include <string.h>
#include <ctype.h>
#include <crc.h>
#include <bootlib.h>
#include <libgen.h>
#include <stdlib.h>
#include "efi_private.h"

/*-- from_shell ---------------------------------------------------------------
 *
 *      Returns if the image was loaded from the UEFI Shell.
 *
 * Parameters
 *      IN ImageHandle: Image handle.
 *
 * Results
 *      bool
 * ---------------------------------------------------------------------------*/
static bool from_shell(EFI_HANDLE Handle)
{
   VOID *proto;
   EFI_STATUS Status;
   EFI_GUID guid = EFI_SHELL_PARAMETERS_PROTOCOL_GUID;

   Status = get_protocol_interface(Handle, &guid, (void **)&proto);

   return Status == EFI_SUCCESS;
}

/*-- get_firmware_info ---------------------------------------------------------
 *
 *      Return the EFI interface information.
 *
 * Parameters
 *      IN firmware: pointer to the firmware info structure to be filled up
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 * ---------------------------------------------------------------------------*/
int get_firmware_info(firmware_t *firmware)
{
   EFI_STATUS Status;
   char *vendor;

   EFI_ASSERT(st != NULL);

   vendor = NULL;
   if (st->FirmwareVendor != NULL) {
      Status = ucs2_to_ascii(st->FirmwareVendor, &vendor, false);
      if (EFI_ERROR(Status)) {
         return error_efi_to_generic(Status);
      }
   }

   memset(firmware, 0, sizeof (firmware_t));
   firmware->interface = FIRMWARE_INTERFACE_EFI;
   firmware->version.efi.major = st->Hdr.Revision >> 16;
   firmware->version.efi.minor = st->Hdr.Revision & 0xffff;
   firmware->vendor = vendor;
   firmware->revision = st->FirmwareRevision;

   return error_efi_to_generic(EFI_SUCCESS);
}

/*-- efi_set_watchdog_timer ----------------------------------------------------
 *
 *      Reset the UEFI watchdog timer. Setting the Timeout parameter to zero
 *      disables the watchdog timer. The timer can be re-enabled by resetting it
 *      with a non-zero Timeout value.
 *
 *      Note: efi_set_watchdog_timer() returns successfully if no watchdog timer
 *            is supported on the platform.
 *
 * Parameters
 *      IN Timeout: expiration time, in seconds
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS efi_set_watchdog_timer(UINTN Timeout)
{
   EFI_STATUS Status;

   EFI_ASSERT(bs != NULL);
   EFI_ASSERT_FIRMWARE(bs->SetWatchdogTimer != NULL);

   Status = bs->SetWatchdogTimer(Timeout, 0, 0, NULL);

   if (Status == EFI_UNSUPPORTED) {
      return ERR_SUCCESS;
   }

   if (EFI_ERROR(Status)) {
      Log(LOG_WARNING, "Could not %s the UEFI watchdog timer.",
          (Timeout == WATCHDOG_DISABLE) ? "disable" : "reset");
   }

   return Status;
}

/*-- firmware_reset_watchdog ---------------------------------------------------
 *
 *      Reset the watchdog timer, if any, to the default timeout.
 *----------------------------------------------------------------------------*/
void firmware_reset_watchdog()
{
   efi_set_watchdog_timer(WATCHDOG_DEFAULT_TIMEOUT);
}

/*-- chainload_parent ----------------------------------------------------------
 *
 *      Transfer execution back to the parent process.
 *
 * Parameters
 *      IN cmdline: unused on UEFI systems
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int chainload_parent(UNUSED_PARAM(const char *cmdline))
{
   EFI_STATUS Status;

   EFI_ASSERT(bs != NULL);
   EFI_ASSERT_FIRMWARE(bs->Exit != NULL);

   Status = bs->Exit(ImageHandle, EFI_SUCCESS, 0, NULL);

   return error_efi_to_generic(Status);
}

/*-- urldecode -----------------------------------------------------------------
 *
 *      URL decode a string.
 *
 * Parameters
 *      IN string: URL-encoded string
 *
 * Results
 *      URL-decoded string
 *
 *----------------------------------------------------------------------------*/
char *urldecode(const char *string)
{
   const char *p;
   char *result, *q;
   char tmp[3];

   result = sys_malloc(strlen(string)); // at least long enough
   p = string;
   q = result;
   while (*p != '\0') {
      if (p[0] == '%' && isxdigit(p[1]) && isxdigit(p[2])) {
         tmp[0] = p[1];
         tmp[1] = p[2];
         tmp[2] = '\0';
         *q++ = strtoul(tmp, NULL, 16);
         p += 3;
      } else {
         *q++ = *p++;
      }
   }
   *q = '\0';
   return result;
}


/*-- efi_create_argv ----------------------------------------------------------
 *
 *      Create an argv-like array from either of the following, if applicable.
 *
 *      (1) The query string in the URL that the image was loaded from.  In
 *      this case argv[0] is the portion of the URL basename that precedes the
 *      "?" query string separator, while argv[1] and following are parsed from
 *      the query string.  In the query string, arguments are separated by '&'
 *      characters.  Each argument is URL-decoded.
 *
 *      (2) The LoadOptions that were passed to the image.  Arguments are
 *      separated by whitespace. NOTE: UEFI Specification v2.3 (8.1. "EFI
 *      Loaded Image Protocol") says "LoadOptions is a pointer to the image's
 *      binary load options".  Unfortunately, the exact format of these options
 *      is not standardized.  In some cases LoadOptions has been observed to
 *      contain a binary GUID, while in other cases it contains a UCS2 command
 *      line.  The command line may or may not contain the name of the image as
 *      its first word, and if the name is there, it may or may not include a
 *      pathname.  We use some best-effort heuristics to construct an argv
 *      array with the image name in argv[0] -- and not in argv[1]!  In some
 *      cases we may place an empty string in argv[0].
 *
 *      In both cases, to include a separator in an argument, the argument can
 *      be quoted with either single or double quote characters.
 *
 * Parameters
 *      IN Image: pointer to the Image protocol interface
 *      OUT argc: number of command line parameters
 *      OUT argv: pointer to the command line parameters array
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS efi_create_argv(EFI_HANDLE Handle,
                           const EFI_LOADED_IMAGE *Image,
                           int *argcp, char ***argvp)
{
   CHAR16 *CommandLine;
   EFI_STATUS Status = EFI_SUCCESS;
   char *cmdline_options = NULL, *path, *p;
   char *bn;
   char **args;
   int status;
   bool bnpresent;
   bool run_from_shell;
   bool is_url_query = false;

   EFI_ASSERT_PARAM(Image != NULL);

#ifdef DEBUG
   log_init(true);
   serial_log_init(DEFAULT_SERIAL_COM, DEFAULT_SERIAL_BAUDRATE);
#endif /* DEBUG */

   *argvp = NULL;
   *argcp = 0;

   run_from_shell = from_shell(Handle);

   /* Get the boot file's basename. */
   status = get_boot_file(&path);
   if (status != ERR_SUCCESS) {
      return error_generic_to_efi(status);
   }
   bn = basename(path);
#ifdef DEBUG
   Log(LOG_DEBUG, "boot_file=%s basename=%s", path, bn);
#endif

   /* Check for URL query string and split from basename. */
   if (strstr(path, "://") != NULL &&
       (p = strchr(bn, '?')) != NULL) {
      *p = ' ';
      is_url_query = true;
      cmdline_options = bn;

   } else {
      /*
       * Check for LoadOptions.  In general, a loaded image doesn't know the
       * data type of its LoadOptions unless it knows what loaded it.
       * Unfortunately, many boot managers will pass a binary GUID in
       * LoadOptions for boot options that were automatically created by the
       * firmware, but the EFI apps in this package want to be able to accept a
       * string of command-line options in the LoadOptions.  To work around
       * this issue, silently ignore the LoadOptions if they are not
       * null-terminated or if conversion from UCS-2 to ASCII finds any
       * characters outside the 8-bit ASCII range.
       */
      CommandLine = Image->LoadOptions;
      if (CommandLine != NULL && Image->LoadOptionsSize > 0 &&
          CommandLine[Image->LoadOptionsSize / sizeof(CHAR16) - 1] == L'\0') {
         Status = ucs2_to_ascii(CommandLine, &cmdline_options, true);
         if (EFI_ERROR(Status) && Status != EFI_INVALID_PARAMETER) {
            goto error;
         }
      }
   }
#ifdef DEBUG
   Log(LOG_DEBUG, "is_url_query=%u cmdline_options=%s",
       is_url_query, cmdline_options);
#endif

   if (cmdline_options != NULL) {
      /* create a tentative argv[] without bn */
      status = str_to_argv(cmdline_options, argcp, argvp, is_url_query);
      Status = error_generic_to_efi(status);
      if (EFI_ERROR(Status)) {
         goto error;
      }
   }

   args = *argvp;

   if (*argcp == 0) {
      bnpresent = false;
   } else if (is_url_query) {
      /*
       * When taking arguments from the URL query string, the whole command
       * line was parsed from bn, so argv[0] must already be the command name.
       * The arguments still need to copied out of path (which is freed below)
       * and URL-decoded.
       */
      int i;
      bnpresent = true;
      for (i = 1; i < *argcp; i++) {
         args[i] = urldecode(args[i]);
      }
   } else if (run_from_shell) {
      /*
       * When running from the shell, argv[0] is always present,
       * and we may have been invoked as 'foo' instead of 'foo.efi'.
       */
      bnpresent = true;
   } else {
      /* Check if bn occurs at the end of argv[0] preceded either by a
       * path delimiter or nothing.
       */
      int lbn = strlen(bn);
      int largv0 = strlen(args[0]);
      bnpresent = lbn > 0 && largv0 >= lbn &&
         strcasecmp(&args[0][largv0 - lbn], bn) == 0 &&
         (largv0 == lbn ||
          args[0][largv0 - lbn - 1] == '/' ||
          args[0][largv0 - lbn - 1] == '\\');
   }

   /* insert bn as argv[0] if not already present */
   if (!bnpresent) {
      char **tmp = sys_malloc((*argcp + 1) * sizeof(char *));
      memcpy(tmp + 1, args, *argcp * sizeof(char *));
      tmp[0] = strdup(bn);
      sys_free(args);
      args = *argvp = tmp;
      (*argcp)++;
#ifdef DEBUG
      Log(LOG_DEBUG, "inserted argv[0]=%s", bn);
#endif
   }

#ifdef DEBUG
   {
      int i;
      Log(LOG_DEBUG, "Dumping passed parameters\n");
      for (i = 0; i < *argcp; i++) {
         Log(LOG_DEBUG, "argv[%u] = '%s'\n", i, args[i]);
      }
   }
#endif /* DEBUG */

 error:
   sys_free(path);

   return Status;
}

/*-- efi_destroy_argv --------------------------------------------------------
 *
 *      Free an argv array allocated with efi_create_argv().
 *
 *      NOTE: The argv array is created in such a way that, argv[0] points to
 *      a contiguous memory area that contains all of the argv[*] strings
 *      separated by '\0's. Thus before freeing the argv array, it's necessary
 *      to free argv[0].
 *
 *      XXX Bug: When efi_create_argv hits the !bnpresent case, it violates the
 *      assumptions of this function.  In that case the contiguous memory area
 *      becomes argv[1] and argv[0] is separately allocated.  So this code will
 *      leak argv[1] in that case.
 *
 * Parameters
 *      IN argv: pointer to the argv array
 *----------------------------------------------------------------------------*/
void efi_destroy_argv(char **argv)
{
   if (argv != NULL) {
      sys_free(argv[0]);
      sys_free(argv);
   }
}

/*******************************************************************************
 * Copyright (c) 2008-2018 VMware, Inc.  All rights reserved.
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
#include "efi_private.h"

EXTERN int main(int argc, char **argv);

EFI_BOOT_SERVICES *bs = NULL;
EFI_RUNTIME_SERVICES *rs = NULL;
EFI_SYSTEM_TABLE *st = NULL;
EFI_HANDLE ImageHandle;

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
      efi_log(LOG_WARNING, "Could not %s the UEFI watchdog timer.\n",
              (Timeout == WATCHDOG_DISABLE) ? "disable" : "reset");
   }

   return Status;
}

/*-- exit_boot_services --------------------------------------------------------
 *
 *      Exit UEFI boot services.
 *
 * Parameters
 *      IN  desc_extra_mem: amount of extra memory (in bytes) needed for each
 *                          memory map descriptor
 *      OUT mmap:           pointer to the E820 system memory map
 *      OUT count:          number of entry in the memory map
 *      OUT efi_info:       EFI information
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *
 * Side Effects
 *      Shutdown the boot services and invalidate the 'bs' global pointer.
 *      EFI Boot Services are no longer available after a call to this function.
 *----------------------------------------------------------------------------*/
int exit_boot_services(size_t desc_extra_mem, e820_range_t **mmap,
                       size_t *count, efi_info_t *efi_info)
{
   EFI_STATUS Status;
   int error;

   EFI_ASSERT(st != NULL);
   EFI_ASSERT(bs != NULL);
   EFI_ASSERT_FIRMWARE(bs->ExitBootServices != NULL);

   if ((efi_info->quirks & EFI_NET_DEV_DISABLE) != 0) {
      disable_network_controllers();
   }

   /*
    * UEFI Specification v2.3 (6.4. "Image Services", ExitBootServices()) says:
    *
    * "An EFI OS loader must ensure that it has the system's current memory map
    *  at the time it calls ExitBootServices(). This is done by passing in the
    *  current memory map's MapKey value as returned by GetMemoryMap().
    *  Care must be taken to ensure that the memory map does not change between
    *  these two calls. It is suggested that GetMemoryMap() be called
    *  immediately before calling ExitBootServices()."
    */
again:
   error = get_memory_map(desc_extra_mem, mmap, count, efi_info);
   if (error != ERR_SUCCESS) {
      return error;
   }

   Status = bs->ExitBootServices(ImageHandle, MapKey);
   if (Status == EFI_INVALID_PARAMETER) {
      free_memory_map(*mmap, efi_info);
      Log(LOG_DEBUG, "...must retry ExitBootServices\n");
      goto again;
   }
   if (!EFI_ERROR(Status)) {
      bs = NULL;

      /*
       * UEFI Specification v2.3 (6.4. "Image Services") says:
       *
       * "On ExitBootServices() success, several fields of the EFI System Table
       *  should be set to NULL. These include ConsoleInHandle, ConIn,
       *  ConsoleOutHandle, ConOut, StandardErrorHandle, StdErr, and
       *  BootServicesTable. In addition, since fields of the EFI System Table
       *  are being modified, the 32-bit CRC for the EFI System Table must be
       *  recomputed."
       */
      st->ConsoleInHandle = NULL;
      st->ConIn = NULL;
      st->ConsoleOutHandle = NULL;
      st->ConOut = NULL;
      st->StandardErrorHandle = NULL;
      st->StdErr = NULL;
      st->BootServices = NULL;
      st->Hdr.CRC32 = 0;
      st->Hdr.CRC32 = crc_32(&st->Hdr, st->Hdr.HeaderSize);

      efi_info->systab = PTR_TO_UINT64(st);
      efi_info->systab_size = st->Hdr.HeaderSize;
      efi_info->valid = true;
   }

   return error_efi_to_generic(Status);
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

/*-- efi_create_argv ----------------------------------------------------------
 *
 *      Create an argv-like array from the Boot Options that have been
 *      passed to the UEFI boot Image.
 *
 *      NOTE: UEFI Specification v2.3 (8.1. "EFI Loaded Image Protocol") says
 *      "LoadOptions is a pointer to the image's binary load options".
 *      Unfortunately, the exact format of these options is not standardized.
 *      In some cases LoadOptions has been observed to contain a binary GUID,
 *      while in other cases it contains a UCS2 command line.  The command line
 *      may or may not contain the name of the image as its first word, and if
 *      the name is there, it may or may not include a pathname.  We use some
 *      best-effort heuristics to construct an argv array with the image name
 *      in argv[0] -- and not in argv[1]!  In some cases we may place an empty
 *      string in argv[0].
 *
 * Parameters
 *      IN Image: pointer to the Image protocol interface
 *      OUT argc: number of command line parameters
 *      OUT argv: pointer to the command line parameters array
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
static EFI_STATUS efi_create_argv(EFI_HANDLE Handle,
                                  const EFI_LOADED_IMAGE *Image,
                                  int *argcp, char ***argvp)
{
   CHAR16 *CommandLine;
   EFI_STATUS Status = EFI_SUCCESS;
   char *cmdline_options = NULL, *path;
   const char *bn;
   char **args;
   int status;
   bool bnpresent;
   bool run_from_shell;

   EFI_ASSERT_PARAM(Image != NULL);

#ifdef DEBUG
   log_init(true);
#endif /* DEBUG */

   *argvp = NULL;
   *argcp = 0;

   run_from_shell = from_shell(Handle);

   /* get the boot file's basename, used as argv[0] if needed */
   status = get_boot_file(&path);
   if (status != ERR_SUCCESS) {
      return error_generic_to_efi(status);
   }
   bn = basename(path);

   /* get the remaining argv[*] if any */
   /*
    * In general, a loaded image doesn't know the data type of its
    * LoadOptions unless it knows what loaded it.  Unfortunately, many
    * boot managers will pass a binary GUID in LoadOptions for boot
    * options that were automatically created by the firmware, but the
    * EFI apps in this package want to be able to accept a string of
    * command-line options in the LoadOptions.  To work around this
    * issue, silently ignore the LoadOptions if they are not
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
   Log(LOG_DEBUG, "boot_file=%s basename=%s cmdline_options=%s",
       path, bn, cmdline_options);

   if (cmdline_options != NULL) {
      /* create a tentative argv[] without bn */
      status = str_to_argv(cmdline_options, argcp, argvp);
      Status = error_generic_to_efi(status);
      if (EFI_ERROR(Status)) {
         goto error;
      }
   }

   args = *argvp;

   /* insert bn as argv[0] if not already present */
   if (*argcp == 0) {
      bnpresent = false;
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

   if (!bnpresent) {
      char **tmp = sys_malloc((*argcp + 1) * sizeof(char *));
      memcpy(tmp + 1, args, *argcp * sizeof(char *));
      tmp[0] = strdup(bn);
      sys_free(args);
      args = *argvp = tmp;
      (*argcp)++;
      Log(LOG_DEBUG, "inserted argv[0]=%s", bn);
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
 * Parameters
 *      IN argv: pointer to the argv array
 *----------------------------------------------------------------------------*/
static void efi_destroy_argv(char **argv)
{
   if (argv != NULL) {
      sys_free(argv[0]);
      sys_free(argv);
   }
}

/*-- efi_main ------------------------------------------------------------------
 *
 *      This is the EFI-specific application entry point. This function
 *      initializes the firmware interface:
 *        1. Setup the ImageHandle, st and bs global variables so they point
 *           respectively to the current image handle, the UEFI system table,
 *           and the UEFI Boot Services dispatch structure.
 *        2. Reset the UEFI watchdog timer to 5 minutes.
 *        3. Initialize the memory allocator.
 *        4. Retrieve the command line arguments.
 *        5. Initialize ACPI support.
 *
 * Parameters
 *      IN Handle:      handle to the parent image
 *      IN SystemTable: pointer to the EFI system table
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS EFIAPI efi_main(EFI_HANDLE Handle, EFI_SYSTEM_TABLE *SystemTable)
{
   EFI_LOADED_IMAGE *Image;
   char **argv;
   int argc;
   int retval;
   EFI_STATUS Status;

   if (Handle == NULL || SystemTable == NULL ||
       SystemTable->BootServices == NULL) {
      return EFI_INVALID_PARAMETER;
   }

   ImageHandle = Handle;
   st = SystemTable;
   bs = st->BootServices;
   rs = st->RuntimeServices;

   efi_set_watchdog_timer(WATCHDOG_DEFAULT_TIMEOUT);

   Status = image_get_info(Handle, &Image);
   if (EFI_ERROR(Status)) {
      return Status;
   }

   mem_init(Image->ImageDataType);

   Status = efi_create_argv(Handle, Image, &argc, &argv);
   if (Status != EFI_SUCCESS) {
      return Status;
   }

   acpi_tab_init();

   retval = main(argc, argv);
   Status = error_generic_to_efi(retval);

   efi_destroy_argv(argv);
   efi_clean_vbe();

   return Status;
}

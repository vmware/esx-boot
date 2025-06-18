/*******************************************************************************
 * Copyright (c) 2008-2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * init.c -- COM32 init/cleanup functions
 */

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <bios.h>
#include <e820.h>
#include <bootlib.h>

#include "com32_private.h"

#define COM32_CLEANUP_ALL      0
#define DEFAULT_BOOT_DRIVE     0x80

EXTERN int main(int argc, char **argv);

com32_t com32;

/*-- com32_get_version ---------------------------------------------------------
 *
 *      Wrapper for the 'Get version' COM32 service.
 *
 * Parameters
 *      OUT fn_max:     number of INT 22h API functions available
 *      OUT major:      Syslinux major version number
 *      OUT minor:      Syslinux minor version number
 *      OUT derivative: Syslinux derivative ID (e.g. 32h = PXELINUX)
 *      OUT version:    Syslinux version string
 *      OUT copyright:  Syslinux copyright string
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 * ---------------------------------------------------------------------------*/
static int com32_get_version(uint16_t *fn_max, uint8_t *major, uint8_t *minor,
                             uint8_t *derivative, const char **version,
                             const char **copyright)
{
   com32sys_t iregs, oregs;
   farptr_t fptr;
   int status;

   memset(&iregs, 0, sizeof (iregs));
   iregs.eax.w[0] = 0x01;
   status = intcall_check_CF(COM32_INT, &iregs, &oregs);
   if (status != ERR_SUCCESS) {
      return status;
   }

   *fn_max = oregs.eax.w[0];
   *major = oregs.ecx.b[1];
   *minor = oregs.ecx.b[0];
   *derivative = oregs.edx.b[0];
   fptr.real.segment = oregs.es;
   fptr.real.offset = oregs.esi.w[0];
   *version = real_to_virtual(fptr);
   fptr.real.offset = oregs.edi.w[0];
   *copyright = real_to_virtual(fptr);

   return ERR_SUCCESS;
}

/*-- com32_cleanup--------------------------------------------------------------
 *
 *      Wrapper for the 'Perform final cleanup' COM32 service.
 *
 * Parameters
 *      IN flags: derivative-specific flags
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 * ---------------------------------------------------------------------------*/
static int com32_cleanup(uint16_t flags)
{
   com32sys_t iregs;

   memset(&iregs, 0, sizeof (iregs));
   iregs.eax.w[0] = 0x0c;
   iregs.edx.w[0] = flags;

   return intcall_check_CF(COM32_INT, &iregs, NULL);
}

/*-- get_firmware_info ---------------------------------------------------------
 *
 *      Return the COM32 interface information.
 *
 * Parameters
 *      IN firmware: pointer to the firmware info structure to be filled up
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 * ---------------------------------------------------------------------------*/
int get_firmware_info(firmware_t *firmware)
{
   const char *derivative;
   char *vendor;

   if (isGPXE()) {
      derivative = "gPXE";
   } else if (isPxelinux()) {
      derivative = "pxelinux";
   } else if (isIsolinux()) {
      derivative = "isolinux";
   } else if (isSyslinux()) {
      derivative = "syslinux";
   } else if (isExtlinux()) {
      derivative = "extlinux";
   } else {
      derivative = "Unknown derivative";
   }

   vendor = strdup(derivative);
   if (vendor == NULL) {
      return ERR_OUT_OF_RESOURCES;
   }

   memset(firmware, 0, sizeof (firmware_t));
   firmware->interface = FIRMWARE_INTERFACE_COM32;
   firmware->version.com32.major = com32.major;
   firmware->version.com32.minor = com32.minor;
   firmware->vendor = vendor;

   return ERR_SUCCESS;
}

/*-- in_boot_services ----------------------------------------------------------
 *
 *      Return true if Syslinux services are still available.
 *
 * Results
 *      true or false.
 *----------------------------------------------------------------------------*/
bool in_boot_services(void)
{
   return com32.in_boot_services;
}

/*-- exit_boot_services --------------------------------------------------------
 *
 *      Exit syslinux boot services.
 *
 * Parameters
 *      IN  desc_extra_mem: extra size needed for each entry (in bytes)
 *      OUT mmap:           pointer to the E820 system memory map
 *      OUT count:          number of entry in the memory map
 *      OUT efi_info_t:     any EFI-related information
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *
 * Side Effects
 *      Syslinux services are no longer available after a call to this function.
 *----------------------------------------------------------------------------*/
int exit_boot_services(size_t desc_extra_mem, e820_range_t **mmap,
                       size_t *count, efi_info_t *efi_info)
{
   int status;

   status = get_memory_map(desc_extra_mem, mmap, count, efi_info);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = com32_cleanup(COM32_CLEANUP_ALL);
   if (status != ERR_SUCCESS) {
      return status;
   }
   com32.in_boot_services = false;

   efi_info->valid = false;

   return ERR_SUCCESS;
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
int chainload_parent(const char *cmdline)
{
   char *bin, *options;
   int status;

   bin = strdup(cmdline);
   if (bin == NULL) {
      return ERR_OUT_OF_RESOURCES;
   }

   options = strchr(bin, ' ');
   if (options != NULL) {
      *options++ = '\0';
   }

   status = firmware_file_exec(bin, options);
   free(bin);

   return status;
}

/*-- firmware_reset_watchdog ---------------------------------------------------
 *
 *      Reset the watchdog timer, if any, to the default timeout.
 *----------------------------------------------------------------------------*/
void firmware_reset_watchdog()
{
   // There is no watchdog timer on com32
}

/*-- com32_get_boot_drive ------------------------------------------------------
 *
 *      Get the boot drive number.
 *
 * Parameters
 *      OUT drive: boot drive number.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 * ---------------------------------------------------------------------------*/
static int com32_get_boot_drive(uint8_t *drive)
{
   com32sys_t iregs, oregs;
   int status;

   if (isSyslinux() || isExtlinux() || isIsolinux()) {
      memset(&iregs, 0, sizeof (iregs));
      iregs.eax.w[0] = 0x0a;
      status = intcall_check_CF(COM32_INT, &iregs, &oregs);
      if (status != ERR_SUCCESS) {
         return status;
      }

      *drive = oregs.edx.b[0];
   } else {
      *drive = DEFAULT_BOOT_DRIVE;
   }

   return ERR_SUCCESS;
}

/*-- com32_create_argv ---------------------------------------------------------
 *
 *      Convert the COM32 command line to an argv-like array.
 *
 *      NOTE: PXELINUX before 3.86 do not provide the module name in the COM32
 *            arguments structure. In that case the boot filename is an empty
 *            string.
 *
 * Parameters
 *      OUT argc:     number of command line parameters
 *      OUT argv:     pointer to the command line parameters array
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int com32_create_argv(int *argc, char ***argv)
{
   const char *argv0;
   char *cmdline;
   char **args;
   int status;

   argv0 = com32_get_modname();
   if (__com32.cs_cmdline != NULL) {
      status = asprintf(&cmdline, "%s %s", argv0, __com32.cs_cmdline);
   } else {
      status = asprintf(&cmdline, "%s", argv0);
   }

   if (status == -1) {
      return ERR_OUT_OF_RESOURCES;
   }

   status = str_to_argv(cmdline, argc, argv, false);
   if (status != ERR_SUCCESS) {
      free(cmdline);
      return status;
   }

   if (*argc > 0 && strcmp(argv0, FAKE_ARGV0) == 0) {
      args = *argv;
      *args[0] = '\0';
   }

   return ERR_SUCCESS;
}

/*-- com32_destroy_argv --------------------------------------------------------
 *
 *      Free an argv array allocated with com32_create_argv().
 *
 *      NOTE: The argv array is created in such a way that, argv[0] points to
 *      a contiguous memory area that contains all of the argv[*] strings
 *      separated by '\0's. Thus before freeing the argv array, it's necessary
 *      to free argv[0].
 *
 * Parameters
 *      IN argv: pointer to the argv array
 *----------------------------------------------------------------------------*/
static void com32_destroy_argv(char **argv)
{
   free(argv[0]);
   free(argv);
}

/*-- com32_main ----------------------------------------------------------------
 *
 *      Initialize syslinux services.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int com32_main(void)
{
   const char *version, *copyright;
   int argc, status;
   uint16_t fn_max;
   char **argv;

   memset(&com32, 0, sizeof (com32));
   com32.in_boot_services = true;

   status = com32_get_version(&fn_max, &com32.major, &com32.minor,
                              &com32.derivative, &version, &copyright);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = com32_get_boot_drive(&com32.drive);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = com32_create_argv(&argc, &argv);
   if (status != ERR_SUCCESS) {
      return status;
   }

   /*
    * esxboot on x86 does not rely on any ACPI tables (e.g. SPCR),
    * so don't bother initializing ACPI yet. When a need arises,
    * don't forget to provide a non-dummy get_acpi_rsdp implementation.
    *
    * acpi_init().
    */

   status = main(argc, argv);

   com32_destroy_argv(argv);

   return status;
}

/*-- relocate_runtime_services --------------------------------------------------
 *
 *      No-op on legacy BIOS.
 *
 * Parameters
 *      IN efi_info: EFI information (should be NULL).
 *
 * Results
 *      ERR_SUCCESS.
 *----------------------------------------------------------------------------*/
int relocate_runtime_services(UNUSED_PARAM(efi_info_t *efi_info),
                              UNUSED_PARAM(bool no_rts),
                              UNUSED_PARAM(bool no_quirks))
{
   return ERR_SUCCESS;
}

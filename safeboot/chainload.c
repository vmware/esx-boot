/*******************************************************************************
 * Copyright (c) 2008-2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * chainload.c -- mboot chainloading
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <boot_services.h>
#include "safeboot.h"

#if defined(__COM32__)
#  define MBOOT_PATH                "mboot.c32"
#else
#  if defined(only_arm64)
#     define MBOOT_PATH             "/EFI/VMware/mboot64.efi"
#  elif defined(only_em64t)
#     define MBOOT_PATH             "/EFI/VMware/mboot64.efi"
#  else
#     define MBOOT_PATH             "/EFI/VMware/mboot32.efi"
#  endif
#endif

/* ESX before v5.0 do not support Multiboot memory map extensions. */
#define FIRST_ESX_WITH_EXTENDED_E820    5
#define FIRST_ESX_WITH_MBI_MMAP_EXTENSION    5

/*-- get_ESX_version_major -----------------------------------------------------
 *
 *      Return the major number of the version of ESX which is installed on a
 *      given boot bank.
 *
 * Parameter
 *      IN  bank:  pointer to the boot bank info structure
 *      OUT major: the ESX major number
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int get_ESX_version_major(bootbank_t *bank, uint32_t *major)
{
   char *build;
   int i;

   build = strdup(bank->build);
   if (build == NULL) {
      return ERR_OUT_OF_RESOURCES;
   }

   for (i = 0; build[i] != '\0'; i++) {
      if (!isdigit(build[i])) {
         build[i] = '\0';
         break;
      }
   }

   if (i == 0) {
      free(build);
      Log(LOG_ERR, "ESX version is unknown.\n");
      return ERR_INCOMPATIBLE_VERSION;
   }

   *major = atoi(build);

   free(build);

   return ERR_SUCCESS;
}

/*-- locate_safeboot -----------------------------------------------------------
 *
 *      Returns an absolute path to safeboot.
 *
 * Parameters
 *      OUT filepath: pointer to the freshsly allocated absolute path
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int locate_safeboot(char **filepath)
{
   char *bootdir, *path;
   int status;

   if (filepath == NULL) {
      return ERR_INVALID_PARAMETER;
   }
   if (safeboot.self_path == NULL || safeboot.self_path[0] == '\0') {
      return ERR_INVALID_PARAMETER;
   }

   status = get_boot_dir(&bootdir);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = make_path(bootdir, safeboot.self_path, &path);
   free(bootdir);
   if (status != ERR_SUCCESS) {
      return status;
   }

   *filepath = path;

   return ERR_SUCCESS;
}

/*-- build_mboot_cmdline -------------------------------------------------------
 *
 *      Prepare the command line that needs to be executed in order to chainload
 *      mboot from the selected boot bank.
 *
 * Parameters
 *      IN  bank:        pointer to the boot bank info structure
 *      IN  next_loader: path to the binary to chainload
 *      OUT options:     pointer to the freshly allocated mboot command line
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int build_mboot_cmdline(bootbank_t *bank, const char *next_loader,
                               char **options)
{
   char *cmdline, *uuid, *chainload, *serial_opts;
   char *safeboot_path = NULL;
   const char *title;
   uint32_t esx_major;
   int len, status;

   status = get_ESX_version_major(bank, &esx_major);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = vmfat_uuid_to_str(bank->uuid, &uuid);
   if (status != ERR_SUCCESS) {
      return status;
   }

   if (safeboot.serial) {
      len = asprintf(&serial_opts, " -S %d -s %d",
                     safeboot.serial_com, safeboot.serial_speed);
      if (len == -1) {
         free(uuid);
         return ERR_OUT_OF_RESOURCES;
      }
   } else {
      serial_opts = NULL;
   }

   if (bank->upgrading) {
      title = "Please wait while VMware Hypervisor is initializing...";
   } else {
      title = "Loading VMware Hypervisor";
   }

   status = locate_safeboot(&safeboot_path);
   if (status != ERR_SUCCESS) {
      free(uuid);
      free(serial_opts);
      return status;
   }

   len = asprintf(&chainload, "-R \"%s%s%s -r -m %s\"",
                  safeboot_path,
                  safeboot.verbose ? " -V" : "",
                  safeboot.serial ? serial_opts : "",
                  next_loader);

   free(safeboot_path);
   if (len == -1) {
      free(uuid);
      free(serial_opts);
      return ERR_OUT_OF_RESOURCES;
   }

   len = asprintf(&cmdline, "%s%s -p %d -c %s%s%s -t \"%s\"%s bootUUID=%s%s",
                  chainload,
                  safeboot.verbose ? " -V" : "",
                  bank->volid,
                  SAFEBOOT_CFG,
                  safeboot.serial ? serial_opts : "",
                  (esx_major >= FIRST_ESX_WITH_MBI_MMAP_EXTENSION) ? "" : " -a",
                  title,
                  safeboot.extra_args,
                  uuid,
                  safeboot.rollback ? " rollback" : "");

   free(serial_opts);
   free(uuid);
   free(chainload);

   if (len == -1) {
      return ERR_OUT_OF_RESOURCES;
   }

   *options = cmdline;

   return ERR_SUCCESS;
}

/*-- chainload -----------------------------------------------------------------
 *
 *      Chainload mboot.
 *
 * Parameters
 *      IN  bank:   pointer to the bank to book from
 *      OUT retval: mboot's return value
 *
 * Results:
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int chainload(bootbank_t *bank, int *retval)
{
   char *next_loader;
   char *cmdline = NULL;
   int status;

   next_loader = safeboot.next_loader;
   if (next_loader == NULL) {
      next_loader = strdup(MBOOT_PATH);
      if (next_loader == NULL) {
         return ERR_OUT_OF_RESOURCES;
      }
   }

   status = build_mboot_cmdline(bank, next_loader, &cmdline);
   if (status == ERR_SUCCESS) {
      Log(LOG_DEBUG, "EXEC: %s\n", cmdline);

      *retval = firmware_file_exec(next_loader, cmdline);
      free(cmdline);
   }

   if (next_loader != safeboot.next_loader) {
      free(next_loader);
   }

   return status;
}

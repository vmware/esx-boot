/*******************************************************************************
 * Copyright (c) 2008-2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * volume.c -- Volume management
 */

#include <string.h>
#include <bootlib.h>
#include <libgen.h>
#include "com32_private.h"

/*-- get_boot_file ------------------------------------------------------------
 *
 *      Get the pathname of the boot file.
 *
 *      NOTE: PXELINUX before 3.86 do not provide the module name in the COM32
 *            arguments structure. In that case the boot filename is an empty
 *            string.
 *
 * Parameter
 *      OUT buffer: pointer to the freshly allocated file path
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int get_boot_file(char **buffer)
{
   char *path, *modname;

   if (buffer == NULL) {
      return ERR_INVALID_PARAMETER;
   }

   modname = (char *)com32_get_modname();
   if (strcmp(modname, FAKE_ARGV0) == 0) {
      path = strdup("");
   } else {
      path = strdup(modname);
   }

   if (path == NULL) {
      return ERR_OUT_OF_RESOURCES;
   }

   *buffer = path;

   return ERR_SUCCESS;
}

/*-- get_boot_dir --------------------------------------------------------------
 *
 *      Get the pathname of the boot directory.
 *
 * Parameter
 *      OUT buffer: pointer to the freshly allocated path
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int get_boot_dir(char **buffer)
{
   char *dirpath, *path;
   int status;

   if (buffer == NULL) {
      return ERR_INVALID_PARAMETER;
   }

   status = get_boot_file(&path);
   if (status != ERR_SUCCESS) {
      return status;
   }
   dirpath = strdup(dirname(path));
   free(path);
   if (dirpath == NULL) {
      return ERR_OUT_OF_RESOURCES;
   }

   if (strcmp(dirpath, ".") == 0) {
      /* If the boot file name provided by COM32 is an empty string or isn't
       * a directory path, but just a filename, we assume root as the boot
       * directory (and to achieve this dirpath is an empty string)
       */
      *dirpath = '\0';
   }

   *buffer = dirpath;

   return ERR_SUCCESS;
}

/*-- get_boot_disk -------------------------------------------------------------
 *
 *      Get the boot device (hard drive, USB stick, CDROM drive, network
 *      device...) information.
 *
 * Parameter
 *      IN disk: pointer to a disk info structure
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int get_boot_disk(disk_t *disk)
{
   return get_disk_info(com32.drive, disk);
}

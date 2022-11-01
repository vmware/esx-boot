/*******************************************************************************
 * Copyright (c) 2021 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * acpi.c -- ACPI table loading
 */

#include <string.h>
#include <stdio.h>
#include <libgen.h>
#include <boot_services.h>
#include "mboot.h"

/*-- uninstall_acpi_tables -----------------------------------------------------
 *
 *      Uninstall previously installed ACPI tables.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
void uninstall_acpi_tables(void)
{
   unsigned int i;

   for (i = 0; i < boot.acpitab_nr; i++) {
      acpi_uninstall_table(boot.acpitab[i].key);
      boot.acpitab[i].is_installed = false;
      boot.acpitab[i].key = 0;
   }
}

/*-- install_acpitab -----------------------------------------------------------
 *
 *      Install a single ACPI table entry into memory.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int install_acpitab(unsigned int n)
{
   const char *filepath;
   size_t size;
   void *addr;
   int status;

   filepath = boot.acpitab[n].filename;
   Log(LOG_INFO, "Installing %s\n", filepath);

   status = file_load(boot.volid, filepath, NULL, &addr, &size);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = acpi_install_table(addr, size, &boot.acpitab[n].key);
   boot.acpitab[n].is_installed = status == ERR_SUCCESS;

   sys_free(addr);

   return status;
}

/*-- install_acpi_tables -------------------------------------------------------
 *
 *      Install ACPI tables into memory.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int install_acpi_tables(void)
{
   unsigned int i;
   int status;

#ifdef SECURE_BOOT
   /*
    * Installing ACPI tables with secure boot enabled is not currently
    * supported. If we ever need to support secure boot on a system where
    * ACPI tables need to be distributed, each table could be signed much
    * like other modules. Then, only unsigned ACPI tables would become a
    * security violation.
    */
   if (boot.efi_info.secure_boot && boot.acpitab_nr > 0) {
      return ERR_SECURITY_VIOLATION;
   }
#endif

   for (i = 0; i < boot.acpitab_nr; i++) {
      status = install_acpitab(i);
      if (status != ERR_SUCCESS) {
         Log(LOG_ERR, "Error %d (%s) while loading ACPI table: %s.\n",
             status, error_str[status], boot.acpitab[i].filename);
         return status;
      }
   }

   return ERR_SUCCESS;
}

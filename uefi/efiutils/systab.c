/*******************************************************************************
 * Copyright (c) 2008-2011,2015,2019 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * systab.c -- EFI system table parsing
 */

#include <string.h>
#include <sm_bios.h>

#include "efi_private.h"

/*-- efi_guid_cmp --------------------------------------------------------------
 *
 *      Compare to EFI GUID.
 *
 * Parameters
 *      IN Guid1: pointer to the first GUID
 *      IN Guid2: pointer to the second GUID
 *
 * Results
 *      0 if both GUID's are equal, a non-zero value otherwise.
 *----------------------------------------------------------------------------*/
int efi_guid_cmp(EFI_GUID *guid1, EFI_GUID *guid2)
{
    return memcmp(guid1, guid2, sizeof (EFI_GUID)) ? 1 : 0;
}

/*-- efi_get_system_config_table -----------------------------------------------
 *
 *      Get a configuration table base address.
 *
 * Parameters
 *      IN  guid:  pointer to the configuration GUID
 *      OUT table: pointer to the table base address
 *
 * Results
 *      EFI_SUCCESS, or an UEFI error status.
 *----------------------------------------------------------------------------*/
static EFI_STATUS efi_get_system_config_table(EFI_GUID *guid, void **table)
{
   unsigned int i;

   EFI_ASSERT(st != NULL);

   *table = NULL;
   for (i = 0; i < st->NumberOfTableEntries; i++) {
      if (efi_guid_cmp(&st->ConfigurationTable[i].VendorGuid, guid) == 0) {
         *table = st->ConfigurationTable[i].VendorTable;
         break;
      }
   }

   return (*table == NULL) ? EFI_NOT_FOUND : EFI_SUCCESS;
}

/*-- get_acpi_rsdp -------------------------------------------------------------
 *
 *      Get the ACPI RSDP. The returned RSDP may be ACPI 1.0 or 2.0+ compliant;
 *      preference is given to a 2.0+ table if it exists.
 *
 *      UEFI Specification v2.3 (Section 4.6 "Configuration table") says:
 *      "ACPI 2.0 or newer tables should use EFI_ACPI_TABLE_GUID"
 *
 * Parameters
 *      OUT rsdp: pointer to the RSDP
 *
 * Results
 *      ERR_SUCCESS: ACPI RSDP found.
 *      ERR_NOT_FOUND: ACPI RSDP not found (but expected).
 *      ERR_UNSUPPORTED: ACPI RSDP not found (but not required).
  *----------------------------------------------------------------------------*/
int get_acpi_rsdp(void **rsdp)
{
   EFI_STATUS Status;
   int i;
   EFI_GUID guids[3] = {
      EFI_ACPI_TABLE_GUID,
      EFI_ACPI_20_TABLE_GUID,
      ACPI_10_TABLE_GUID
   };

   Status = EFI_NOT_FOUND;

   for (i = 0; i < 3; i++) {
      if (i > 0 && efi_guid_cmp(&guids[0], &guids[i]) == 0) {
         continue;
      }

      Status = efi_get_system_config_table(&guids[i], rsdp);
      if (Status == EFI_SUCCESS) {
         break;
      }
   }

   if (error_efi_to_generic(Status) != ERR_SUCCESS) {
      if (arch_is_arm64) {
         /*
          * On Arm this is a problem, since we rely on ACPI to locate
          * the serial port for console.
          */
         Log(LOG_CRIT, "ACPI expected, but not found, good luck!");
         return ERR_NOT_FOUND;
      } else {
         /*
          * On x86 this is not a problem. While ESXi itself needs ACPI,
          * esxboot doesn't query any tables and will not enforce any
          * checks.
          */
         return ERR_UNSUPPORTED;
      }
   }

   return ERR_SUCCESS;
}

/*-- get_smbios_eps ------------------------------------------------------------
 *
 *      Get the legacy 32-bit SMBIOS Entry Point Structure (EPS).
 *
 * Parameters
 *      OUT eps_start: pointer to starting address of the EPS.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int get_smbios_eps(void **eps_start)
{
   void *eps;
   EFI_STATUS Status;
   EFI_GUID guid = SMBIOS_TABLE_GUID;

   Status = efi_get_system_config_table(&guid, &eps);
   if (Status != EFI_SUCCESS) {
      return error_efi_to_generic(Status);
   }

   *eps_start = eps;
   return ERR_SUCCESS;
}

/*-- get_smbios_v3_eps ---------------------------------------------------------
 *
 *      Get the v3 64-bit SMBIOS Entry Point Structure (EPS).
 *
 * Parameters
 *      OUT eps_start: pointer to starting address of the EPS.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int get_smbios_v3_eps(void **eps_start)
{
   void *eps;
   EFI_STATUS Status;
   EFI_GUID guid = SMBIOS3_TABLE_GUID;

   Status = efi_get_system_config_table(&guid, &eps);
   if (Status != EFI_SUCCESS) {
      return error_efi_to_generic(Status);
   }

   *eps_start = eps;
   return ERR_SUCCESS;
}

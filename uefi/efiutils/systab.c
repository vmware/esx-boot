/*******************************************************************************
 * Copyright (c) 2008-2011,2015 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * systab.c -- EFI system table parsing
 */

#include <string.h>
#include <sm_bios.h>

#include "efi_private.h"
static EFI_ACPI_5_0_ROOT_SYSTEM_DESCRIPTION_POINTER *acpi_rsdp;
static EFI_ACPI_DESCRIPTION_HEADER *acpi_rsdt;
static EFI_ACPI_DESCRIPTION_HEADER *acpi_xsdt;
EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE *acpi_spcr;

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
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int get_acpi_rsdp(void **rsdp)
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

   return error_efi_to_generic(Status);
}

/*-- acpi_tab_init -------------------------------------------------------------
 *
 *      Find ACPI tables, storing pointers to interesting tables if they exist.
 *
 * Parameters
 *      None
 *
 * Results
 *      None
 *----------------------------------------------------------------------------*/
void acpi_tab_init(void)
{
   int status;
   void *entry;
   uintptr_t tab;
   uintptr_t tab_end;
   uintptr_t entry_size;
   EFI_ACPI_DESCRIPTION_HEADER *header;

   status = get_acpi_rsdp((void **) &acpi_rsdp);
   if (status != ERR_SUCCESS) {
      return;
   }

   if (acpi_rsdp->Revision >= EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER_REVISION) {
      acpi_xsdt = (EFI_ACPI_DESCRIPTION_HEADER *) (uintptr_t) acpi_rsdp->XsdtAddress;
   }

   acpi_rsdt = (EFI_ACPI_DESCRIPTION_HEADER *) (uintptr_t) acpi_rsdp->RsdtAddress;
   if (acpi_xsdt != 0) {
      tab = (uintptr_t) acpi_xsdt;
      entry_size = sizeof(uint64_t);
      tab_end = tab + acpi_xsdt->Length;
   } else {
      tab = (uintptr_t) acpi_rsdt;
      entry_size = sizeof(uint32_t);
      tab_end = tab + acpi_rsdt->Length;
   }

   for (tab += sizeof(EFI_ACPI_DESCRIPTION_HEADER);
        tab < tab_end;
        tab += entry_size) {
      entry = (void *) tab;

      if (entry_size == sizeof(uint32_t)) {
         header = (EFI_ACPI_DESCRIPTION_HEADER *) (uintptr_t) *(uint32_t *) entry;
      } else {
         header = (EFI_ACPI_DESCRIPTION_HEADER *) (uintptr_t) *(uint64_t *) entry;
      }

      if (header == NULL) {
         Log(LOG_DEBUG, "NULL SDT entry detected\n");
         continue;
      }

      if (header->Signature == EFI_ACPI_5_0_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_SIGNATURE) {
         acpi_spcr = (EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE *) header;
      }
   }
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

/*******************************************************************************
 * Copyright (c) 2021 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * acpi_table.c -- EFI ACPI table protocol support.
 */

#include "efi_private.h"

static EFI_ACPI_TABLE_PROTOCOL *acpi_table = NULL;

/*-- firmware_init_acpi_table --------------------------------------------------
 *
 *      Initialize ACPI table protocol.
 *
 * Parameters
 *      None
 *
 * Results
 *      None
 *----------------------------------------------------------------------------*/
void
firmware_init_acpi_table(void)
{
   EFI_GUID AcpiTableProto = EFI_ACPI_TABLE_PROTOCOL_GUID;
   EFI_STATUS status;

   if (acpi_table != NULL) {
      return;
   }

   status = LocateProtocol(&AcpiTableProto, (void **)&acpi_table);
   if (EFI_ERROR(status)) {
      acpi_table = NULL;
      return;
   }

   Log(LOG_DEBUG, "ACPI table protocol detected");
}

/*-- firmware_install_acpi_table -----------------------------------------------
 *
 *      Install an ACPI table into the RSDT/XSDT.
 *
 * Parameters
 *      IN buffer: pointer to the ACPI table
 *      IN size:   size of the ACPI table in bytes
 *      OUT key:   key used to refer to the ACPI table when uninstalling
 *
 * Results
 *      ERR_SUCCESS, or an error status.
 *----------------------------------------------------------------------------*/
int
firmware_install_acpi_table(void *buffer, size_t size, unsigned int *key)
{
   EFI_STATUS status;

   if (acpi_table == NULL) {
      return ERR_UNSUPPORTED;
   }

   status = acpi_table->InstallAcpiTable(acpi_table, buffer, size,
                                         (UINTN *)key);
   if (EFI_ERROR(status)) {
      return error_efi_to_generic(status);
   }

   return ERR_SUCCESS;
}

/*-- firmware_uninstall_acpi_table ---------------------------------------------
 *
 *      Removes an ACPI table from the RSDT/XSDT.
 *
 * Parameters
 *      IN key: specifies the table to uninstall, returned by
 *              firmware_install_acpi_table()
 *
 * Results
 *      ERR_SUCCESS, or an error status.
 *----------------------------------------------------------------------------*/
int
firmware_uninstall_acpi_table(unsigned int key)
{
   EFI_STATUS status;

   if (acpi_table == NULL) {
      return ERR_UNSUPPORTED;
   }

   status = acpi_table->UninstallAcpiTable(acpi_table, key);
   if (EFI_ERROR(status)) {
      return error_efi_to_generic(status);
   }

   return ERR_SUCCESS;
}

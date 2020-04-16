/*******************************************************************************
 * Copyright (c) 2008-2011,2019 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * systab.c -- System table parsing
 */

#include <string.h>
#include <bios.h>
#include <bootlib.h>

#include "com32_private.h"

typedef struct {
   uint32_t base;
   uint32_t len;
} addr32_range_t;

/*-- find_system_table ---------------------------------------------------------
 *
 *      Locate a system table in the given ranges of memory.
 *
 * Parameters
 *      IN  scan:        array of memory address ranges to be scanned
 *      IN  paragraph:   the table must be aligned on this boundary
 *      IN  size:        table minimum size
 *      IN  check_table: pointer to a function that checks whether a valid table
 *                       is present at a given address
 *      OUT table:       pointer to the table
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int find_system_table(addr32_range_t *scan, uint32_t paragraph,
                             size_t size, int (*check_table)(void *),
                             void **table)
{
   uint32_t len;
   char *p;

   while (scan->len > 0) {
      len = scan->len;

      if (len > paragraph) {
         p = UINT64_TO_PTR(roundup64(scan->base, paragraph));
         len -= (ptrdiff_t)p - (ptrdiff_t)scan->base;

         while (len >= size) {
            if (check_table(p) == ERR_SUCCESS) {
               *table = p;
               return ERR_SUCCESS;
            }

            p += paragraph;
            len -= paragraph;
         }
      }

      scan++;
   }

   return ERR_NOT_FOUND;
}

/*-- smbios_check_eps ----------------------------------------------------------
 *
 *      Check the legacy 32-bit SMBIOS Entry Point Structure (EPS) integrity:
 *        - table signature is correct
 *        - checksum(s) make the structure bytes add up to zero
 *
 * Parameters
 *      IN eps: pointer to the EPS
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int smbios_check_eps(void *eps)
{
   smbios_eps *p = eps;

   if (memcmp(p->anchor_string, SMBIOS_EPS_SIGNATURE,
              SMBIOS_EPS_SIGNATURE_LEN) == 0) {
      if (!is_valid_firmware_table(p, p->length)) {
         return ERR_CRC_ERROR;
      }
   } else {
      return ERR_NOT_FOUND;
   }

   return ERR_SUCCESS;
}

/*-- smbios_check_v3_eps -------------------------------------------------------
 *
 *      Check the v3 64-bit SMBIOS Entry Point Structure (EPS) integrity:
 *        - table signature is correct
 *        - checksum(s) make the structure bytes add up to zero
 *
 * Parameters
 *      IN eps: pointer to the EPS
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int smbios_check_v3_eps(void *eps)
{
   smbios_eps3 *p3 = eps;

   if (memcmp(p3->anchor_string, SMBIOS_EPS3_SIGNATURE,
              SMBIOS_EPS3_SIGNATURE_LEN) == 0) {
      if (!is_valid_firmware_table(p3, p3->length)) {
         return ERR_CRC_ERROR;
      }
   } else {
      return ERR_NOT_FOUND;
   }

   return ERR_SUCCESS;
}

/*-- get_smbios_eps ------------------------------------------------------------
 *
 *      Get the legacy 32-bit SMBIOS Entry Point Structure (EPS).
 *
 *      On non-EFI systems, the SMBIOS Entry Point structure, described below,
 *      can be located by application software by searching for the anchor-
 *      string on paragraph (16-byte) boundaries within the physical memory
 *      address range 000F0000h to 000FFFFFh.
 *
 * Parameters
 *      OUT eps_start: pointer to starting address of the EPS.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int get_smbios_eps(void **eps_start)
{
   int ret;
   smbios_eps *eps;

   addr32_range_t scan[2] = {
      {0xf0000, BIOS_UPPER_MEM_START - 0xf0000},
      {0, 0}
   };

   ret = find_system_table(scan, SMBIOS_PARAGRAPH_SIZE,
                           sizeof (smbios_eps),
                           smbios_check_eps, (void **) &eps);
   if (ret != ERR_SUCCESS) {
      return ret;
   }

   *eps_start = eps;
   return ERR_SUCCESS;
}


/*-- get_smbios_v3_eps ---------------------------------------------------------
 *
 *      Get the v3 64-bit SMBIOS Entry Point Structure (EPS).
 *
 *      On non-EFI systems, the SMBIOS Entry Point structure, described below,
 *      can be located by application software by searching for the anchor-
 *      string on paragraph (16-byte) boundaries within the physical memory
 *      address range 000F0000h to 000FFFFFh.
 *
 * Parameters
 *      OUT eps_start: pointer to starting address of the EPS.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int get_smbios_v3_eps(void **eps_start)
{
   int ret;
   smbios_eps3 *eps;

   addr32_range_t scan[2] = {
      {0xf0000, BIOS_UPPER_MEM_START - 0xf0000},
      {0, 0}
   };

   ret = find_system_table(scan, SMBIOS_PARAGRAPH_SIZE,
                           sizeof (smbios_eps3),
                           smbios_check_v3_eps, (void **) &eps);
   if (ret != ERR_SUCCESS) {
      return ret;
   }

   *eps_start = eps;
   return ERR_SUCCESS;
}


/*-- get_acpi_rsdp -------------------------------------------------------------
 *
 *      Get the ACPI RSDP.
 *
 * Parameters
 *      OUT rsdp: pointer to the RSDP
 *
 * Results
 *      ERR_NOT_SUPPORTED, as ACPI info is not used or checked in BIOS mode
 *      on x86 systems.
 *----------------------------------------------------------------------------*/
int get_acpi_rsdp(UNUSED_PARAM(void **rsdp))
{
   return ERR_UNSUPPORTED;
}

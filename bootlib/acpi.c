/*******************************************************************************
 * Copyright (c) 2019 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * acpi.c -- ACPI-related routines.
 */

#include <stdint.h>
#include <boot_services.h>
#include <acpi.h>
#include <bootlib.h>

static uintptr_t tab = 0;
static uintptr_t tab_end = 0;
static uintptr_t entry_size;

/*-- acpi_is_present -----------------------------------------------------------
 *
 *      Return if system firmware provided ACPI support.
 *
 * Parameters
 *      None.
 *
 * Results
 *      ERR_SUCCESS: ACPI found.
 *      ERR_NOT_FOUND: ACPI not found (but expected).
 *      ERR_UNSUPPORTED: ACPI not found (but not required).
 *----------------------------------------------------------------------------*/
int acpi_is_present(void)
{
   void *rsdp;

   return get_acpi_rsdp((void **) &rsdp);
}

/*-- acpi_matches_sdt ----------------------------------------------------------
 *
 *      Checks if a table matches signature.
 *
 * Parameters
 *      IN sdt: ACPI table header
 *      IN sig: 4-byte signature as string (e.g. "SPCR")
 *
 * Results
 *      true or false.
 *----------------------------------------------------------------------------*/
static bool acpi_matches_sdt(const acpi_sdt *sdt, const char *sig)
{
   char *d = (char *) &sdt->signature;

#ifdef DEBUG
   char *o = (char *) &sdt->oem_id;
   char *t = (char *) &sdt->table_id;
   LOG(LOG_DEBUG, "Looking at %c%c%c%c OEM <%c%c%c%c%c%c> "
       "Product <%c%c%c%c%c%c%c%c>", d[0], d[1], d[2], d[3],
       o[0], o[1], o[2], o[3], o[4], o[5],
       t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7]);
#endif /* DEBUG */

   if (memcmp(d, sig, sizeof(sdt->signature)) == 0) {
      return true;
   }

   return false;
}

/*-- acpi_find_sdt -------------------------------------------------------------
 *
 *      Returns the first ACPI table that matches signature.
 *
 * Parameters
 *      IN sig: 4-byte signature as string (e.g. "SPCR")
 *
 * Results
 *      Matched table or NULL.
 *----------------------------------------------------------------------------*/
acpi_sdt *acpi_find_sdt(const char *sig)
{
   uintptr_t entry;
   acpi_sdt *header;

   for (entry = tab; entry < tab_end; entry += entry_size) {
      if (entry_size == sizeof(uint32_t)) {
         header = (acpi_sdt *) (uintptr_t) *(uint32_t *) ((void *) entry);
      } else {
         header = (acpi_sdt *) (uintptr_t) *(uint64_t *) ((void *) entry);
      }

      if (header == NULL) {
         LOG(LOG_DEBUG, "NULL SDT entry detected");
         continue;
      }

      if (acpi_matches_sdt(header, sig)) {
         return header;
      }
   }

   return NULL;
}

/*-- acpi_init -----------------------------------------------------------------
 *
 *      Find ACPI tables, storing pointers to interesting tables if they exist.
 *
 * Parameters
 *      None
 *
 * Results
 *      None
 *----------------------------------------------------------------------------*/
void acpi_init(void)
{
   int status;
   acpi_rsdp *rsdp;
   acpi_sdt *rsdt;
   acpi_sdt *xsdt = NULL;

   status = get_acpi_rsdp((void **) &rsdp);
   if (status != ERR_SUCCESS) {
      LOG(LOG_DEBUG, "No ACPI present");
      return;
   }

   if (rsdp->revision >= ACPI_RSDP_V2) {
      xsdt = (acpi_sdt *) (uintptr_t) rsdp->xsdt_address;
   }

   rsdt = (acpi_sdt *) (uintptr_t) rsdp->rsdt_address;
   if (xsdt != 0) {
      tab = (uintptr_t) xsdt;
      entry_size = sizeof(uint64_t);
      tab_end = tab + xsdt->length;
      LOG(LOG_DEBUG, "XSDT @ %"PRIxPTR"-%"PRIxPTR"", tab, tab_end);
   } else {
      tab = (uintptr_t) rsdt;
      entry_size = sizeof(uint32_t);
      tab_end = tab + rsdt->length;
      LOG(LOG_DEBUG, "RSDT @ %"PRIxPTR"-%"PRIxPTR"", tab, tab_end);
   }
   tab += sizeof(acpi_sdt);
}

/*******************************************************************************
 * Copyright (c) 2016-2018 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * quirks.c -- EFI quirks support.
 */

#include <string.h>
#include <ctype.h>
#include <crc.h>
#include <bootlib.h>
#include <libgen.h>
#include "efi_private.h"

#define SANITIZE_STRP(x) (x = (x == NULL) ? "" : x)

static struct quirk {
   const char *manufacturer;
   const char *product;
   const char *bios_ver;  /* optional */
   const char *bios_date; /* optional */
   uint64_t efi_quirks;
} quirks[] = {
   {
      "Dell Inc.", "PowerEdge T320", NULL, NULL,
      EFI_RTS_OLD_AND_NEW | EFI_RTS_UNKNOWN_MEM
   },
   {
      "GIGABYTE", "MT30-GS2-00", "T48", NULL,
      EFI_FB_BROKEN
   },
   /*
    * Must be last.
    */
   {
      NULL, NULL, NULL, NULL, 0
   }
};

#define last_quirk(q) ((q)->manufacturer == NULL &&   \
                       (q)->product == NULL)

/*-- check_efi_quirks ----------------------------------------------------------
 *
 *      Process any device-specific quirks.
 *
 * Parameters
 *      IN efi_info: EFI info structure to be updated.
 *
 * Results
 *      None.
 * ---------------------------------------------------------------------------*/
void check_efi_quirks(efi_info_t *efi_info)
{
   void *eps_start;
   size_t eps_length;
   void *table_start;
   size_t table_length;
   smbios_entry smbios_start;
   smbios_entry smbios_end;
   smbios_entry type0;
   smbios_entry type1;
   const char *manufacturer;
   const char *product;
   const char *bios_ver;
   const char *bios_date;
   struct quirk *q = quirks;

   if (st->FirmwareVendor != NULL &&
       ucs2_strcmp(st->FirmwareVendor, L"Apple") == 0) {
      /*
       * Work around Mac mini bug where a network device keeps DMAing to
       * memory after ExitBootServices. Apple rdar://problem/9303938,
       * VMware PR 910787, 1131266.  Avoid doing this on non-Apple
       * hardware, VMware PR 1424506.
       */
      efi_info->quirks |= EFI_NET_DEV_DISABLE;
   }

   if (smbios_get_v3_info(&eps_start, &eps_length,
                          &table_start, &table_length) != ERR_SUCCESS) {
      if (smbios_get_info(&eps_start, &eps_length,
                          &table_start, &table_length) != ERR_SUCCESS) {
         Log(LOG_DEBUG, "No SMBIOS to match quirks on\n");
         return;
      }
   }

   smbios_start.raw_bytes = table_start;
   smbios_end.raw_bytes = smbios_start.raw_bytes + table_length;

   if (smbios_get_struct(smbios_start, smbios_end, 1, &type1) != ERR_SUCCESS) {
      /*
       * No type 1?
       */
      return;
   }

   manufacturer = smbios_get_string(type1, smbios_end,
                             type1.type1->manufacturer);
   product = smbios_get_string(type1, smbios_end,
                               type1.type1->product_name);
   SANITIZE_STRP(manufacturer);
   SANITIZE_STRP(product);

   /*
    * Treat type 0 data as optional.
    */
   type0.raw_bytes = NULL;
   bios_ver = NULL;
   bios_date = NULL;
   smbios_get_struct(smbios_start, smbios_end, 0, &type0);
   if (type0.raw_bytes != NULL) {
      bios_ver = smbios_get_string(type0, smbios_end,
                                   type0.type0->bios_ver);
      bios_date = smbios_get_string(type0, smbios_end,
                                    type0.type0->bios_date);
   }
   SANITIZE_STRP(bios_ver);
   SANITIZE_STRP(bios_date);

   Log(LOG_DEBUG, "Looking up quirks for %s %s %s %s\n",
       manufacturer, product, bios_ver, bios_date);

   for (; !last_quirk(q); q++) {
      if (strcmp(q->manufacturer, manufacturer) != 0 ||
          strcmp(q->product, product) != 0) {
         continue;
      }

      if (q->bios_ver != NULL &&
          strcmp(q->bios_ver, bios_ver) != 0) {
         continue;
      }

      if (q->bios_date != NULL &&
          strcmp(q->bios_date, bios_date) != 0) {
         continue;
      }

      efi_info->quirks |= q->efi_quirks;
      Log(LOG_DEBUG, "Matched quirks 0x%"PRIx64"\n", efi_info->quirks);
      return;
   }
}

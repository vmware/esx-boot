/*******************************************************************************
 * Copyright (c) 2016-2018 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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

   if (smbios_get_platform_info(&manufacturer, &product, &bios_ver,
                                &bios_date) != ERR_SUCCESS) {
     Log(LOG_DEBUG, "No SMBIOS to match quirks on\n");
     return;
   }

   SANITIZE_STRP(manufacturer);
   SANITIZE_STRP(product);
   SANITIZE_STRP(bios_ver);
   SANITIZE_STRP(bios_date);

   Log(LOG_DEBUG, "Looking up quirks for '%s' '%s' '%s' '%s'\n",
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

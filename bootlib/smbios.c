/*******************************************************************************
 * Copyright (c) 2017-2018, 2021 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * smbios.c -- SMBIOS-related routines.
 */

#include <stdint.h>
#include <boot_services.h>
#include <bootlib.h>

/*-- smbios_get_struct ---------------------------------------------------------
 *
 *      Searching within the memory range bounded by ptr and end, find the next
 *      SMBIOS table entry of matching type.
 *
 * Parameters
 *      IN ptr: entry address to search from.
 *      IN end: entry address to search to.
 *      IN type: entry type to search for.
 *      OUT entry: pointer to address of found SMBIOS table entry.
 *
 * Results
 *      ERR_SUCCESS or ERR_NOT_FOUND.
 *----------------------------------------------------------------------------*/
int smbios_get_struct(smbios_entry ptr,
                      smbios_entry end,
                      uint8_t type,
                      smbios_entry *entry)
{
   while (ptr.raw_bytes < end.raw_bytes) {
      if (ptr.header->type == type) {
         *entry = ptr;
         return ERR_SUCCESS;
      }

      for (ptr.raw_bytes += ptr.header->length;
           ptr.raw_bytes < end.raw_bytes - 1;
           ptr.raw_bytes++) {
         if (ptr.raw_bytes[0] == '\0' &&
             ptr.raw_bytes[1] == '\0') {
            ptr.raw_bytes += 2;
            break;
         }
      }
   }

   return ERR_NOT_FOUND;
}

/*-- smbios_get_string --------------------------------------------------------
 *
 *      Searching within the memory range for an SMBIOS table entry, bounded
 *      by ptr and end, return the string matching the passed string id.
 *
 * Parameters
 *      IN ptr: entry address to search from.
 *      IN end: entry address to search to.
 *      IN type: entry type to search for.
 *      OUT entry: pointer to address of found SMBIOS table entry.
 *
 * Results
 *      String or NULL of no string matching the index was found.
 *----------------------------------------------------------------------------*/
char *smbios_get_string(smbios_entry ptr,
                        smbios_entry end,
                        unsigned index)
{
   unsigned current = 1;

   for (ptr.raw_bytes += ptr.header->length;
        ptr.raw_bytes < end.raw_bytes - 1;
        ptr.raw_bytes++) {
      if (current == index) {
         return (char *) ptr.raw_bytes;
      }

      if (ptr.raw_bytes[0] == '\0') {
         if (ptr.raw_bytes[1] == '\0') {
            /*
             * End of structure. No such string.
             */
            return NULL;
         } else {
            /*
             * This was the delimiter before next string.
             */
            current++;
         }
      }
   }

   return NULL;
}

/*-- smbios_get_info -----------------------------------------------------------
 *
 *      Get the legacy 32-bit SMBIOS Entry Point Structure (EPS) and associated
 *      SMBIOS table info.
 *
 *      A table address of 0 is treated as missing tables.
 *
 * Parameters
 *      OUT eps_start: pointer to starting address of the EPS.
 *      OUT eps_length: pointer to length of the EPS.
 *      OUT table_start: pointer to starting address of SMBIOS table.
 *      OUT table_length: pointer to length of SMBIOS table.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int smbios_get_info(void **eps_start, size_t *eps_length,
                    void **table_start, size_t *table_length)
{
   smbios_eps *eps;
   int ret;

   ret = get_smbios_eps((void **) &eps);
   if (ret != ERR_SUCCESS) {
      return ret;
   }

   if (memcmp(eps, SMBIOS_EPS_SIGNATURE, SMBIOS_EPS_SIGNATURE_LEN) != 0) {
      return ERR_NOT_FOUND;
   }

   if (eps->table_address == 0) {
      return ERR_NOT_FOUND;
   }

   *eps_start = eps;
   *eps_length = eps->length;
   *table_start = (void *) (uintptr_t) eps->table_address;
   *table_length = eps->table_length;
   return ERR_SUCCESS;
}


/*-- smbios_get_v3_info -----------------------------------------------------------
 *
 *      Get the v3 64-bit SMBIOS Entry Point Structure (EPS) and associated
 *      SMBIOS table info.
 *
 *      A table address of 0 is treated as missing tables.
 *
 * Parameters
 *      OUT eps_start: pointer to starting address of the EPS.
 *      OUT eps_length: pointer to length of the EPS.
 *      OUT table_start: pointer to starting address of SMBIOS table.
 *      OUT table_length: pointer to length of SMBIOS table.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int smbios_get_v3_info(void **eps_start, size_t *eps_length,
                       void **table_start, size_t *table_length)
{
   smbios_eps3 *eps;
   int ret;

   ret = get_smbios_v3_eps((void **) &eps);
   if (ret != ERR_SUCCESS) {
      return ret;
   }

   if (memcmp(eps, SMBIOS_EPS3_SIGNATURE, SMBIOS_EPS3_SIGNATURE_LEN) != 0) {
      return ERR_NOT_FOUND;
   }

   if (eps->table_address == 0) {
      return ERR_NOT_FOUND;
   }

   *eps_start = eps;
   *eps_length = eps->length;
   *table_start = (void *) (uintptr_t) eps->table_address;
   /*
    * Don't bother refining down the size, use the maximum possible.
    */
   *table_length = eps->table_max_length;
   return ERR_SUCCESS;
}


/*-- smbios_get_platform_info -----------------------------------------------------
 *
 *      Get the most useful DMI data out: vendor, product and BIOS info.
 *
 * Parameters
 *      OUT manufacturer: pointer to store pointer to manufacturer string,
 *                        if found, or NULL.
 *      OUT product: pointer to store pointer to product string, if found,
 *                   or NULL.
 *      OUT bios_ver: pointer to store pointer to firmware verstion, if found,
 *                    or NULL.
 *      OUT bios_date: pointer to store pointer to firmware build date, if
 *                     found, or NULL.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/

int smbios_get_platform_info(const char **manufacturer,
                             const char **product,
                             const char **bios_ver,
                             const char **bios_date)
{
   void *eps_start;
   size_t eps_length;
   void *table_start;
   size_t table_length;
   smbios_entry smbios_start;
   smbios_entry smbios_end;
   smbios_entry type0;
   smbios_entry type1;

   *manufacturer = *product = *bios_ver = *bios_date = NULL;
   if (smbios_get_v3_info(&eps_start, &eps_length,
                          &table_start, &table_length) != ERR_SUCCESS) {
      if (smbios_get_info(&eps_start, &eps_length,
                          &table_start, &table_length) != ERR_SUCCESS) {
         return ERR_UNSUPPORTED;
      }
   }

   smbios_start.raw_bytes = table_start;
   smbios_end.raw_bytes = smbios_start.raw_bytes + table_length;

   if (smbios_get_struct(smbios_start, smbios_end, 1, &type1) !=
       ERR_SUCCESS) {
      /*
       * No type 1?
       */
      return ERR_NOT_FOUND;
   }

   *manufacturer = smbios_get_string(type1, smbios_end,
                                     type1.type1->manufacturer);
   *product = smbios_get_string(type1, smbios_end,
                                type1.type1->product_name);

   /*
    * Treat type 0 data as optional.
    */
   type0.raw_bytes = NULL;
   smbios_get_struct(smbios_start, smbios_end, 0, &type0);
   if (type0.raw_bytes != NULL) {
      *bios_ver = smbios_get_string(type0, smbios_end,
                                    type0.type0->bios_ver);
      *bios_date = smbios_get_string(type0, smbios_end,
                                     type0.type0->bios_date);
   }

   return ERR_SUCCESS;
}

/*******************************************************************************
 * Copyright (c) 2017-2021 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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


/*-- smbios_get_v3_info --------------------------------------------------------
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


/*-- smbios_get_platform_info --------------------------------------------------
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
   const char *local_manufacturer, *local_product, *local_bios_ver,
      *local_bios_date;
   int status;
   if ((status = smbios_get_firmware_info(&local_bios_ver, &local_bios_date))
      != ERR_SUCCESS) {
      return status;
   }
   if ((status = smbios_get_system_info(&local_manufacturer, &local_product,
         NULL, NULL, NULL, NULL, NULL)) != ERR_SUCCESS) {
      return status;
   }
   *manufacturer = local_manufacturer;
   *product = local_product;
   *bios_ver = local_bios_ver;
   *bios_date = local_bios_date;

   return ERR_SUCCESS;
}


/*-- smbios_get_firmware_info --------------------------------------------------
 *
 *      Get information from the SMBIOS Firmware table.
 *
 * Parameters
 *      OUT bios_ver: pointer to store pointer to firmware verstion, if found,
 *                    or NULL.
 *      OUT bios_date: pointer to store pointer to firmware build date, if
 *                     found, or NULL.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/

int smbios_get_firmware_info (
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

   if (bios_ver != NULL) {
      *bios_ver = NULL;
   }

   if (bios_date != NULL) {
      *bios_date = NULL;
   }

   if (smbios_get_v3_info(&eps_start, &eps_length,
                          &table_start, &table_length) != ERR_SUCCESS) {
      if (smbios_get_info(&eps_start, &eps_length,
                          &table_start, &table_length) != ERR_SUCCESS) {
         return ERR_UNSUPPORTED;
      }
   }

   smbios_start.raw_bytes = table_start;
   smbios_end.raw_bytes = smbios_start.raw_bytes + table_length;

   if (smbios_get_struct(smbios_start, smbios_end, 0, &type0) != ERR_SUCCESS) {
      return ERR_NOT_FOUND;
   }

   if (type0.raw_bytes == NULL) {
      return ERR_LOAD_ERROR;
   }

   if (bios_ver != NULL) {
      *bios_ver =
         smbios_get_string(
            type0, smbios_end,
            type0.type0->bios_ver);
   }

   if (bios_date != NULL) {
      *bios_date =
         smbios_get_string(
            type0, smbios_end,
            type0.type0->bios_date);
   }

   return ERR_SUCCESS;
}


/*-- smbios_get_system_info ----------------------------------------------------
 *
 *      Get information from the SMBIOS system table.
 *
 * Parameters
 *      OUT manufacturer: pointer to store pointer to manufacturer string, if
 *                        found, or NULL.
 *      OUT product_name: pointer to store pointer to product string, if found,
 *                        or NULL.
 *      OUT version: pointer to store pointer to system version string, if
 *                   found, or NULL.
 *      OUT serial_number: pointer to store pointer to system serial number
 *                         string if found, or NULL.
 *      OUT system_uuid: pointer to store pointer to system UUID, if found, or
 *                       NULL.
 *      OUT sku: pointer to store pointer to system sku string, if found, or
 *               NULL.
 *      OUT family: pointer to store pointer to system family string, if found,
 *                  or NULL.
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/

int smbios_get_system_info (
   const char **manufacturer,
   const char **product_name,
   const char **version,
   const char **serial_number,
   const uint8_t *system_uuid[16],
   const char **sku,
   const char **family)
{
   void *eps_start;
   size_t eps_length;
   void *table_start;
   size_t table_length;
   smbios_entry smbios_start;
   smbios_entry smbios_end;
   smbios_entry type1;


   if (manufacturer != NULL) {
      *manufacturer = NULL;
   }

   if (product_name != NULL) {
      *product_name = NULL;
   }

   if (version != NULL) {
      *version = NULL;
   }

   if (serial_number != NULL) {
      *serial_number = NULL;
   }

   if (system_uuid != NULL) {
      *system_uuid = NULL;
   }

   if (sku != NULL) {
      *sku = NULL;
   }

   if (family != NULL) {
      *family = NULL;
   }

   if (smbios_get_v3_info(&eps_start, &eps_length,
                          &table_start, &table_length) != ERR_SUCCESS) {
      if (smbios_get_info(&eps_start, &eps_length,
                          &table_start, &table_length) != ERR_SUCCESS) {
         return ERR_UNSUPPORTED;
      }
   }

   smbios_start.raw_bytes = table_start;
   smbios_end.raw_bytes = smbios_start.raw_bytes + table_length;

   if (smbios_get_struct(smbios_start, smbios_end, 1, &type1) != ERR_SUCCESS) {
      return ERR_NOT_FOUND;
   }

   if (manufacturer != NULL) {
      *manufacturer =
         smbios_get_string(
            type1, smbios_end,
            type1.type1->manufacturer);
   }

   if (product_name != NULL) {
      *product_name =
         smbios_get_string(
            type1, smbios_end,
            type1.type1->product_name);
   }

   if (version != NULL) {
      *version =
         smbios_get_string(
            type1, smbios_end,
            type1.type1->version);
   }

   if (serial_number != NULL) {
      *serial_number =
         smbios_get_string(
            type1, smbios_end,
            type1.type1->serial_number);
   }

   if (system_uuid != NULL) {
      *system_uuid = type1.type1->uuid;
   }

   if (sku != NULL) {
      *sku =
         smbios_get_string(
            type1, smbios_end,
            type1.type1->sku);
   }

   if (family != NULL) {
      *family =
         smbios_get_string(
            type1, smbios_end,
            type1.type1->family);
   }

   return ERR_SUCCESS;
}

/*-- smbios_get_version ---------------------------------------------------
 *
 *      Reads the SMBIOS version from the SMBIOS entry point structure
 *
 * Parameters
 *      OUT major: the address where the major version will be returned
 *         if not NULL.
 *      OUT minor: the address where the minor version will be returned
 *         if not NULL.
 *      OUT docrev: the address where the docrev version will be returned
 *         if not NULL. If the entry point structure doesn't have such a
 *         field, zero is returned.
 * Results
 *      ERR_SUCCESS or a generic error status.
 *----------------------------------------------------------------------------*/
int smbios_get_version(int *major, int *minor, int *doc_rev)
{
   void *eps_start;
   size_t eps_length;
   void *table_start;
   size_t table_length;

   if (smbios_get_v3_info(&eps_start, &eps_length,
                          &table_start, &table_length) == ERR_SUCCESS) {
      smbios_eps3 *entry_point_structure_3 = eps_start;
      if (major != NULL) {
         *major = entry_point_structure_3->major_version;
      }
      if (minor != NULL) {
         *minor = entry_point_structure_3->minor_version;
      }
      if (doc_rev != NULL) {
         *doc_rev = entry_point_structure_3->doc_rev;
      }
   } else if (smbios_get_info(&eps_start, &eps_length,
                              &table_start, &table_length) == ERR_SUCCESS) {
      smbios_eps *entry_point_structure = eps_start;
      if (major != NULL) {
         *major = entry_point_structure->major_version;
      }
      if (minor != NULL) {
         *minor = entry_point_structure->minor_version;
      }
      if (doc_rev != NULL) {
         *doc_rev = 0;
      }
   } else {
      return ERR_UNSUPPORTED;
   }
   return ERR_SUCCESS;
}

/*-- smbios_get_oem_strings ----------------------------------------------------
 *
 *       Get OEM strings from the type 11 structure in the SMBIOS table.
 *       Handles the following corner cases:
 *
 *       corner_case_1: cover the case where there's more than 255 strings in
 *       the unformed section of type11
 *
 *       corner_case_2: cover the case where the formatted section or the
 *       unformed section goes out of the smbios table's bounds
 *
 *       corner_case_3: cover the case where the first string is the empty
 *       string, a.k.a. the unformed section
 *          can be 00430000h , resulting in oem_string_0="", oem_string_1="C"
 *
 *       corner_case_4: cover the case where the first string is the empty
 *       string and the expected oem string count is zero, a.k.a. the unformed
 *       section can be 0000h, resulting in no oem strings, taken from the
 *       smbios spec version 3.7.0, line 887 and 888
 *
 *
 * Parameters
 *      OUT oem: pointer where the result value is returned on success.
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int smbios_get_oem_strings(oem_strings_t *oem_strings)
{
   void *eps_start;
   size_t eps_length;
   void *table_start;
   size_t table_length;
   smbios_entry smbios_start;
   smbios_entry smbios_end;
   smbios_entry type11;
   char *s;
   char *unformed_section_begin;
   oem_strings_t local_oem_strings;
   size_t expected_count;
   int status;

   if (oem_strings == NULL) {
      return ERR_INVALID_PARAMETER;
   }

   if (smbios_get_v3_info(&eps_start, &eps_length,
                          &table_start, &table_length) != ERR_SUCCESS) {
      if (smbios_get_info(&eps_start, &eps_length,
                          &table_start, &table_length) != ERR_SUCCESS) {
         return ERR_UNSUPPORTED;
      }
   }

   smbios_start.raw_bytes = table_start;
   smbios_end.raw_bytes = smbios_start.raw_bytes + table_length;

   /* locate all type11 instances in the SMBIOS table */
   local_oem_strings.length = 0;
   local_oem_strings.names = NULL;
   local_oem_strings.entries = NULL;
   while (smbios_start.raw_bytes < smbios_end.raw_bytes) {
      char **tmp1 = NULL;
      key_value_t *tmp2 = NULL;

      if ((status = smbios_get_struct(smbios_start, smbios_end, 11, &type11)) !=
            ERR_SUCCESS) {
         break;
      }

      if ((type11.type11->header.length < sizeof(smbios_type11)) ||
         /* corner case 2 */
         ((char*) type11.type11 + type11.type11->header.length + 2 /* "\0\0" */
         >
         (char*)(smbios_end.raw_bytes))) {
         status = ERR_INCONSISTENT_DATA;
         goto err1;
      }

      /*
      * iterate over the unformed section and check if it matches type11's
      * counter
      */
      s = unformed_section_begin =
         ((char*)(type11.type11)) + type11.type11->header.length;

      expected_count = local_oem_strings.length + type11.type11->count;

      tmp1 = sys_realloc(local_oem_strings.names,
         local_oem_strings.length * sizeof(local_oem_strings.names[0]),
         expected_count * sizeof(local_oem_strings.names[0]));
      if (tmp1 == NULL) {
         status = ERR_OUT_OF_RESOURCES;
         goto err1;
      }
      local_oem_strings.names = tmp1;

      tmp2 = sys_realloc(local_oem_strings.entries,
         local_oem_strings.length * sizeof(local_oem_strings.entries[0]),
         expected_count * sizeof(local_oem_strings.entries[0]));
      if (tmp2 == NULL) {
         status = ERR_OUT_OF_RESOURCES;
         goto err1;
      }

      for (char *current_string = unformed_section_begin;
         s < (((char*)smbios_end.raw_bytes) - 1);
         ++s) {
         if (s[0] == '\0') {
            if (!((s[1] != '\0') ^ (current_string == unformed_section_begin)))
            {
               if (local_oem_strings.length < expected_count) {
                  char* name;
                  if ((asprintf(&name, "oem_string_%zu",
                     local_oem_strings.length))
                     == -1) {
                     status = ERR_OUT_OF_RESOURCES;
                     goto err1;
                  }
                  local_oem_strings.names[local_oem_strings.length] = name;
                  local_oem_strings.entries[local_oem_strings.length].key =
                     name;
                  local_oem_strings.entries[local_oem_strings.length].value =
                     current_string;
                  local_oem_strings.length++;
               } else {
                  /*
                   * type11 instance has more strings than expected,
                   * ignore them
                   */
                  break;
               }
            }

            if (s[1] == '\0') {
               break;
            }
            current_string = ++s;
         }
      }

      /*
       * Move s to the next SMBIOS struct, after the current unformed section.
       * Cover the corner case where 0000h is not present at the end of the
       * unformed section, thus we hit the smbios_end.raw_bytes limit
       */
      for ( ; s < (((char*)smbios_end.raw_bytes) - 1); ++s) {
         if (s[0] == '\0' && s[1] == '\0') {
            break;
         }
      }
      s += 2;
      smbios_start.raw_bytes = (uint8_t *)s;
   }
   *oem_strings = local_oem_strings;
   return ERR_SUCCESS;

   err1:
   /* cleanup */
   for (size_t i = 0; i < local_oem_strings.length; ++i) {
      free(local_oem_strings.names[i]);
   }
   free(local_oem_strings.entries);
   free(local_oem_strings.names);
   return status;
}

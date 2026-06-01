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

/*-- smbios_get_info_and_version -----------------------------------------------
 *
 *      Retrieve the SMBIOS entry point structure and SMBIOS version from it.
 *      This function will first try to get the V3 64-bit SMBIOS Entry Point
 *      Structure (EPS) and associated SMBIOS table info and if this fails will
 *      try to get the legacy 32-bit EPS and associated SMBIOS table info.
 *      Further it will return the version data from the EPS.
 *
 * Parameters
 *      OUT eps_start: pointer to starting address of the EPS.
 *      OUT eps_length: pointer to length of the EPS.
 *      OUT table_start: pointer to starting address of SMBIOS table.
 *      OUT table_length: pointer to length of SMBIOS table.
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
int smbios_get_info_and_version(
   void **eps_start, size_t *eps_length,
   void **table_start, size_t *table_length,
   uint8_t *version_major, uint8_t *version_minor, uint8_t *doc_rev)
{
   void *local_eps_start = NULL;
   size_t local_eps_length = 0;
   void *local_table_start = NULL;
   size_t local_table_length = 0;

   if ((eps_start != NULL && *eps_start != NULL) ||
       (table_start != NULL && *table_start != NULL)) {
      return ERR_INVALID_PARAMETER;
   }
   eps_start = eps_start ? : &local_eps_start;
   eps_length = eps_length ? : &local_eps_length;
   table_start = table_start ? : &local_table_start;
   table_length = table_length ? : &local_table_length;
   if (smbios_get_v3_info(eps_start, eps_length,
                          table_start, table_length) == ERR_SUCCESS) {
      smbios_eps3 *entry_point_structure_3 = *eps_start;

      if (version_major != NULL) {
         *version_major = entry_point_structure_3->major_version;
      }
      if (version_minor != NULL) {
         *version_minor = entry_point_structure_3->minor_version;
      }
      if (doc_rev != NULL) {
         *doc_rev = entry_point_structure_3->doc_rev;
      }
      return ERR_SUCCESS;
   } else if (smbios_get_info(eps_start, eps_length,
                              table_start, table_length) == ERR_SUCCESS) {
      smbios_eps *entry_point_structure = *eps_start;

      if (version_major != NULL) {
         *version_major = entry_point_structure->major_version;
      }
      if (version_minor != NULL) {
         *version_minor = entry_point_structure->minor_version;
      }
      // doc_rev is not present in the 32-bit entry point structure
      return ERR_SUCCESS;
   }

   return ERR_UNSUPPORTED;
}

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
   int status = ERR_INVALID_PARAMETER;

   if ((manufacturer != NULL && *manufacturer != NULL) ||
       (product      != NULL && *product      != NULL) ||
       (bios_ver     != NULL && *bios_ver     != NULL) ||
       (bios_date    != NULL && *bios_date    != NULL)) {
      return status;
   }
   if ((status = smbios_get_firmware_info(bios_ver, bios_date, NULL))
         != ERR_SUCCESS) {
      return status;
   }
   if ((status = smbios_get_system_info(manufacturer, product,
         NULL, NULL, NULL, NULL, NULL, NULL)) != ERR_SUCCESS) {
      return status;
   }

   return status;
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
 *      OUT smbios_version: pointer to store pointer to smbios version string.
 *                          The returned value is allocated with malloc(...) so
 *                          it should be released using free(...).
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int smbios_get_firmware_info (
   const char **bios_ver,
   const char **bios_date,
   const char **smbios_version)
{
   int status = ERR_INVALID_PARAMETER;
   uint8_t smbios_major = 0;
   uint8_t smbios_minor = 0;

   void *table_start = NULL;
   size_t table_length = 0;
   smbios_entry smbios_start;
   smbios_entry smbios_end;
   smbios_entry type0;

   if ((bios_ver  != NULL && *bios_ver  != NULL) ||
       (bios_date != NULL && *bios_date != NULL) ||
       (smbios_version != NULL && *smbios_version != NULL)) {
      return status;
   }
   if ((status = smbios_get_info_and_version(
         NULL, NULL,
         &table_start, &table_length,
         &smbios_major, &smbios_minor, NULL)) == ERR_SUCCESS) {

      if (smbios_version != NULL) {
         if (asprintf(
               (char**)smbios_version, "%d.%d", smbios_major, smbios_minor)
                  == -1) {
            return ERR_OUT_OF_RESOURCES;
         }
      }

      smbios_start.raw_bytes = table_start;
      smbios_end.raw_bytes = smbios_start.raw_bytes + table_length;

      status = smbios_get_struct(smbios_start, smbios_end, 0, &type0);
      if (status != ERR_SUCCESS) {
         return status;
      }

      if (type0.raw_bytes != NULL) {
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
      }
   }

   return status;
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
   const uuid_t **system_uuid,
   uint8_t *wake_up_type,
   const char **sku,
   const char **family)
{
   int status = ERR_INVALID_PARAMETER;

   void *table_start = NULL;
   size_t table_length = 0;
   uint8_t version_major = 0;
   uint8_t version_minor = 0;
   smbios_entry smbios_start;
   smbios_entry smbios_end;
   smbios_entry type1;

   if ((manufacturer  != NULL && *manufacturer  != NULL) ||
       (product_name  != NULL && *product_name  != NULL) ||
       (version       != NULL && *version       != NULL) ||
       (serial_number != NULL && *serial_number != NULL) ||
       (system_uuid   != NULL && *system_uuid   != NULL) ||
       (sku           != NULL && *sku           != NULL) ||
       (family        != NULL && *family        != NULL)) {
      return status;
   }

   if ((status = smbios_get_info_and_version(
         NULL, NULL,
         &table_start, &table_length,
         &version_major, &version_minor, NULL)) == ERR_SUCCESS) {
      smbios_start.raw_bytes = table_start;
      smbios_end.raw_bytes = smbios_start.raw_bytes + table_length;

      if ((status = smbios_get_struct(smbios_start, smbios_end, 1, &type1))
            == ERR_SUCCESS) {
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
            *system_uuid = &type1.type1->uuid;
         }
         if (wake_up_type != NULL) {
            *wake_up_type = type1.type1->wake_up_type;
         }
         if (version_major > 2 ||
             (version_major == 2 && version_minor >= 4)) {
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
         }
      }
   }

   return status;
}

/*-- smbios_get_chassis_info ---------------------------------------------------
 *
 *      Get information from the SMBIOS chassis table.
 *
 * Parameters
 *      OUT manufacturer: pointer to store pointer to chassis manufacturer
 *                        string, if found, or NULL.
 *      OUT type: pointer to store chassis type value, if found, or 0.
 *      OUT version: pointer to store pointer to chassis version string, if
 *                   found, or NULL.
 *      OUT serial_number: pointer to store pointer to chassis serial number
 *                         string if found, or NULL.
 *      OUT asset_tag: pointer to store pointer to chassis asset tag string, if
 *                     found, or NULL.
 *      OUT boot_up_state: pointer to store chassis boot_up_state value, if
*                          found, or 0.
 *      OUT power_supply_state: pointer to store chassis power_supply_state
 *                              value, if found, or 0.
 *      OUT thermal_state: pointer to store chassis thermal_state value, if
 *                         found, or 0.
 *      OUT security_status: pointer to store chassis security_status value, if
 *                           found, or 0.
 *      OUT oem_defined: pointer to store chassis oem_defined value, if found,
 *                       or 0.
 *      OUT height: pointer to store chassis height value, if found, or 0.
 *      OUT number_of_power_cords: pointer to store chassis
 *                                 number_of_power_cords value, if found, or 0.
 *      OUT element_count: pointer to store chassis element_count value, if
 *                         found, or 0.
 *      OUT element_length: pointer to store chassis element_length value, if
 *                          found, or 0.
 *      OUT elements_data: pointer to store pointer to chassis elements_data, if
 *                         found, or NULL.
 *      OUT sku: pointer to store pointer to chassis sku string, if found, or
 *               NULL.
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int smbios_get_chassis_info (
   const char **manufacturer,
   uint8_t *type,
   const char **version,
   const char **serial_number,
   const char **asset_tag,
   uint8_t *boot_up_state,
   uint8_t *power_supply_state,
   uint8_t *thermal_state,
   uint8_t *security_status,
   uint32_t *oem_defined,
   uint8_t *height,
   uint8_t *number_of_power_cords,
   uint8_t *element_count,
   uint8_t *element_length,
   const uint8_t **elements_data,
   const char **sku)
{
   int status = ERR_INVALID_PARAMETER;
   void *table_start = NULL;
   size_t table_length = 0;
   uint8_t version_major = 0;
   uint8_t version_minor = 0;
   smbios_entry smbios_start;
   smbios_entry smbios_end;
   smbios_entry type3;

   if ((manufacturer != NULL  && *manufacturer != NULL) ||
       (version != NULL       && *version != NULL) ||
       (serial_number != NULL && *serial_number != NULL) ||
       (asset_tag != NULL     && *asset_tag != NULL) ||
       (elements_data != NULL && *elements_data != NULL) ||
       (sku != NULL           && *sku != NULL)) {
      return status;
   }

   if ((status = smbios_get_info_and_version(
         NULL, NULL,
         &table_start, &table_length,
         &version_major, &version_minor, NULL)) == ERR_SUCCESS) {
      smbios_start.raw_bytes = table_start;
      smbios_end.raw_bytes = smbios_start.raw_bytes + table_length;

      if ((status = smbios_get_struct(smbios_start, smbios_end, 3, &type3))
            == ERR_SUCCESS) {
         if (manufacturer != NULL) {
            *manufacturer =
               smbios_get_string(
                  type3, smbios_end,
                  type3.type3->manufacturer);
         }
         if (type != NULL) {
            *type = type3.type3->type;
         }
         if (version != NULL) {
            *version =
               smbios_get_string(
                  type3, smbios_end,
                  type3.type3->version);
         }
         if (serial_number != NULL) {
            *serial_number =
               smbios_get_string(
                  type3, smbios_end,
                  type3.type3->serial_number);
         }
         if (asset_tag != NULL) {
            *asset_tag =
               smbios_get_string(
                  type3, smbios_end,
                  type3.type3->asset_tag);
         }
         if (boot_up_state != NULL) {
            *boot_up_state = type3.type3->boot_up_state;
         }
         if (power_supply_state != NULL) {
            *power_supply_state = type3.type3->power_supply_state;
         }

         if (thermal_state != NULL) {
            *thermal_state = type3.type3->thermal_state;
         }
         if (security_status != NULL) {
            *security_status = type3.type3->security_status;
         }

         if (version_major > 2 ||
             (version_major == 2 && version_minor >= 3)) {
            if (oem_defined != NULL) {
               *oem_defined = type3.type3->oem_defined;
            }
            if (height != NULL) {
               *height = type3.type3->height;
            }
            if (number_of_power_cords != NULL) {
               *number_of_power_cords = type3.type3->number_of_power_cords;
            }
            if (element_count != NULL) {
               *element_count = type3.type3->element_count;
            }
            if (element_length != NULL) {
               *element_length = type3.type3->element_length;
            }
            if (elements_data != NULL) {
               *elements_data = type3.type3->elements_data;
            }

            if (version_major > 2 ||
                (version_major == 2 && version_minor >= 7)) {
               if (sku != NULL) {
                  smbios_string_id id = type3.type3->elements_data[
                     type3.type3->element_count * type3.type3->element_length];
                  *sku = smbios_get_string(type3, smbios_end, id);
               }
            }
         }
      }
   }

   return status;
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
   int status = ERR_INVALID_PARAMETER;
   void *eps_start = NULL;
   size_t eps_length = 0;
   void *table_start = NULL;
   size_t table_length = 0;
   uint8_t version_major = 0;
   uint8_t version_minor = 0;
   smbios_entry smbios_start;
   smbios_entry smbios_end;
   smbios_entry type11;
   char *s;
   char *unformed_section_begin;
   size_t expected_count;

   if (oem_strings == NULL ||
       (oem_strings != NULL &&
         (oem_strings->length != 0 ||
          oem_strings->entries != NULL))) {
      return status;
   }

   if ((status = smbios_get_info_and_version(
         &eps_start, &eps_length,
         &table_start, &table_length,
         &version_major, &version_minor, NULL)) != ERR_SUCCESS) {
      return status;
   }

   smbios_start.raw_bytes = table_start;
   smbios_end.raw_bytes = smbios_start.raw_bytes + table_length;

   /* locate all type11 instances in the SMBIOS table */
   while (smbios_start.raw_bytes < smbios_end.raw_bytes) {
      key_value_t *reallocated_entries = NULL;

      if ((status = smbios_get_struct(smbios_start, smbios_end, 11, &type11))
            != ERR_SUCCESS) {
         /*
          * if there are no (more) structures of type 11 exit reporting
          * success.
          */
         status = ERR_SUCCESS;
         break;
      }

      if ((type11.type11->header.length < sizeof(smbios_type11)) ||
         /* corner case 2 */
         ((char*) type11.type11 + type11.type11->header.length + 2 /* "\0\0" */
         >
         (char*)(smbios_end.raw_bytes))) {
         status = ERR_INCONSISTENT_DATA;
         break;
      }

      /*
       * Iterate over the unformed section and check if it matches type11's
       * counter.
       */
      s = unformed_section_begin =
         ((char*)(type11.type11)) + type11.type11->header.length;

      expected_count = oem_strings->length + type11.type11->count;

      reallocated_entries = sys_realloc(oem_strings->entries,
         oem_strings->length * sizeof(oem_strings->entries[0]),
         expected_count * sizeof(oem_strings->entries[0]));
      if (reallocated_entries == NULL) {
         status = ERR_OUT_OF_RESOURCES;
         break;
      }
      oem_strings->entries = reallocated_entries;

      for (char *current_string = unformed_section_begin;
         s < (((char*)smbios_end.raw_bytes) - 1);
         ++s) {
         if (s[0] == '\0') {
            if (!((s[1] != '\0') ^ (current_string == unformed_section_begin)))
            {
               if (oem_strings->length < expected_count) {
                  oem_strings->entries[oem_strings->length].key = "oemstring";
                  oem_strings->entries[oem_strings->length].value =
                     current_string;
                  oem_strings->length++;
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
   return status;
}

/*-- get_pre_2_6_smbios_uuid_str -----------------------------------------------
 *
 *      Convert a UUID to a human readable string according to the format
 *      defined in SMBIOS specification prior version 2.6. The output string
 *      is allocated with malloc() and should be released with free().
 *
 * Parameters
 *      IN uuid: pointer to the UUID
 *      OUT uuid_str:  pointer to the freshly allocated output string
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int get_pre_2_6_smbios_uuid_str(const uuid_t uuid, char **uuid_str)
{
   return asprintf(
      uuid_str,
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      uuid[0], uuid[1], uuid[2], uuid[3],
      uuid[4], uuid[5],
      uuid[6], uuid[7],
      uuid[8], uuid[9],
      uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]) < 0
         ? ERR_INVALID_PARAMETER : ERR_SUCCESS;
}

/*-- get_2_6_smbios_uuid_str ---------------------------------------------------
 *
 *      Convert a UUID to a human readable string according to the format
 *      defined in SMBIOS specification version 2.6 (and above). The output
 *      string is allocated with malloc() and should be released with free().
 *
 * Parameters
 *      IN uuid: pointer to the UUID
 *      OUT uuid_str:  pointer to the freshly allocated output string
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int get_2_6_smbios_uuid_str(const uuid_t uuid, char **uuid_str)
{
   return asprintf(
      uuid_str,
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      uuid[3], uuid[2], uuid[1], uuid[0],
      uuid[5], uuid[4],
      uuid[7], uuid[6],
      uuid[8], uuid[9],
      uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]) < 0
         ? ERR_INVALID_PARAMETER : ERR_SUCCESS;
}

/*-- get_smbios_uuid_str -------------------------------------------------------
 *
 *      Convert a UUID to a human readable string according to the format
 *      defined in SMBIOS specification where the UUID is formatted according to
 *      the SMBIOS version reported by the host machine. The output
 *      string is allocated with malloc() and should be released with free().
 *
 * Parameters
 *      IN uuid: pointer to the UUID
 *      OUT uuid_str:  pointer to the freshly allocated output string
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int get_smbios_uuid_str(const uuid_t uuid, char **uuid_str)
{
   int status = ERR_INVALID_PARAMETER;
   uint8_t version_major = 0;
   uint8_t version_minor = 0;

   if (uuid_str != NULL && *uuid_str != NULL) {
      return status;
   }

   if ((status = smbios_get_info_and_version(
         NULL, NULL,
         NULL, NULL,
         &version_major, &version_minor, NULL)) != ERR_SUCCESS) {
      return status;
   }

   /* if version < 2.6 , use big endian for all the fields, see PR 3347702 */
   if (version_major < 2 ||
       (version_major == 2 && version_minor < 6)) {
      return get_pre_2_6_smbios_uuid_str(uuid, uuid_str);
   } else {
      return get_2_6_smbios_uuid_str(uuid, uuid_str);
   }
}

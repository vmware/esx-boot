/*******************************************************************************
 * Copyright (c) 2008-2011,2016,2021-2022 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * vmfat.c -- VMware FAT support
 */

#include <string.h>
#include <stdio.h>
#include <boot_services.h>
#include "safeboot.h"

#define VMWARE_FAT_MAGIC            "VMWARE FAT16    "
#define VMWARE_FAT_MAGIC_LEN        16

/*-- vmfat_uuid_to_str ---------------------------------------------------------
 *
 *      Convert a VMware FAT UUID to a human readable string where each byte in
 *      the UUID is seen as an unsigned char and represented by a 0-prefixed,
 *      2-characters, lower case hexadecimal string. The output string is
 *      allocated with sys_malloc() and can be freed with sys_free().
 *
 * Parameters
 *      IN uuid: pointer to the VMware FAT UUID
 *      IN str:  pointer to the freshly allocated output string
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int vmfat_uuid_to_str(const unsigned char *uuid, char **str)
{
   int i, len, status;
   char *s;

   ASSERT(uuid != NULL);
   ASSERT(str != NULL);

   status = str_alloc(2 * VMWARE_FAT_UUID_LEN, &s);
   if (status != ERR_SUCCESS) {
      return status;
   }

   *str = s;

   for (i = 0; i < VMWARE_FAT_UUID_LEN; i++) {
      len = snprintf(s, 3, "%02x", (unsigned int)uuid[i]);
      if (len != 2) {
         sys_free(s);
         return ERR_INVALID_PARAMETER;
      }
      s += 2;
   }

   return ERR_SUCCESS;
}

/*-- vmfat_get_uuid ------------------------------------------------------------
 *
 *      Read the VMware FAT UUID from the specified partition.
 *
 * Parameters
 *      IN volid:  MBR/GPT partition number
 *      IN buffer: pointer to the output buffer
 *      IN buflen: output buffer size, in bytes
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int vmfat_get_uuid(int volid, void *buffer, size_t buflen)
{
   uint8_t *block;
   uint8_t *uuid;
   disk_t disk;
   partition_t partition;
   int status;

   if (buffer == NULL || buflen < VMWARE_FAT_UUID_LEN) {
      return ERR_INVALID_PARAMETER;
   }

   status = get_boot_disk(&disk);
   if (status != ERR_SUCCESS) {
      return status;
   }

   /*
    * Return an error if this is not a valid partition type to contain a FAT
    * filesystem; see PR 2678561.  Only PART_TYPE_FAT16 and PART_TYPE_EFI are
    * really expected to contain a FAT filesystem on an ESXi-created boot disk,
    * but we tolerate other MBR partition types that can contain FAT just in
    * case.  For GPT, gpt_to_partinfo translates only the expected GUID
    * partition types to PART_TYPE_FAT16 or PART_TYPE_EFI; others translate to
    * PART_TYPE_NON_FS or ERR_NO_MEDIA and thus are rejected here.
    */
   status = get_volume_info(&disk, volid, &partition);
   if (status != ERR_SUCCESS) {
      return status;
   }
   switch (partition.info.type) {
   case PART_TYPE_FAT16:
   case PART_TYPE_EFI:
      break;

   case PART_TYPE_FAT12:
   case PART_TYPE_FAT16_LT32MB:
   case PART_TYPE_FAT32:
   case PART_TYPE_FAT32_LBA:
   case PART_TYPE_FAT16_LBA:
      break;

   case PART_TYPE_EMPTY:
      return ERR_NO_MEDIA;

   case PART_TYPE_EXTENDED:
   case PART_TYPE_WIN_EXTENDED:
   case PART_TYPE_LINUX_EXTENDED:
   case PART_TYPE_NON_FS:
   case PART_TYPE_GPT_PROTECTIVE:
   default:
      return ERR_BAD_TYPE;
   }

   /*
    * The UUID is present in the second sector of the disk.
    * First determine the sector size to get the required offset
    * to read the UUID from.
    */
   block = sys_malloc(disk.bytes_per_sector);
   if (block == NULL) {
      return ERR_OUT_OF_RESOURCES;
   }

   status = volume_read(&disk, &partition, block, disk.bytes_per_sector,
                        disk.bytes_per_sector);
   if (status != ERR_SUCCESS) {
      sys_free(block);
      return status;
   }

   if (memcmp(VMWARE_FAT_MAGIC, block, VMWARE_FAT_MAGIC_LEN)) {
      sys_free(block);
      return ERR_NOT_FOUND;
   }

   uuid = &block[VMWARE_FAT_MAGIC_LEN];
   memcpy(buffer, uuid, VMWARE_FAT_UUID_LEN);

   sys_free(block);
   return ERR_SUCCESS;
}

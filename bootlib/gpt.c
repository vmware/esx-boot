/*******************************************************************************
 * Copyright (c) 2008-2011,2018,2021-2022 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * gpt.c -- GUID Partition Table (GPT)
 */

#include <bootlib.h>
#include <boot_services.h>
#include <string.h>
#include <zlib.h>

// Adapted from edk2
typedef struct {
  uint32_t  data1;
  uint16_t  data2;
  uint16_t  data3;
  uint8_t   data4[8];
} GUID;

#define GPT_SIGNATURE   0x5452415020494645ULL

#define GPT_UNUSED_PARTITION_GUID \
   { 0x00000000, 0x0000, 0x00, \
      { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }

#define GPT_BASIC_DATA_PARTITION_GUID \
   { 0xEBD0A0A2, 0xB9E5, 0x4433, \
      { 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7 } }

#define GPT_EFI_SYSTEM_PARTITION_GUID \
   { 0xC12A7328, 0xF81F, 0x11D2, \
      { 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B } }

typedef struct {
   uint64_t signature;
   uint32_t revision;
   uint32_t headerSize;
   uint32_t headerCrc32;
   uint32_t reserved;
   uint64_t myLba;
   uint64_t alternateLba;
   uint64_t firstUsableLba;
   uint64_t lastUsableLba;
   GUID diskGuid;
   uint64_t entryArrayLba;
   uint32_t numberOfEntries;
   uint32_t sizeOfEntry;
   uint32_t entryArrayCrc32;
} gpt_header;

typedef struct {
   GUID type;
   GUID guid;
   uint64_t start_lba;
   uint64_t end_lba;
   uint64_t attributes;
   uint16_t name[36];
} gpt_entry;

static const GUID unused_partition_guid = GPT_UNUSED_PARTITION_GUID;
static const GUID basic_data_partition_guid = GPT_BASIC_DATA_PARTITION_GUID;
static const GUID efi_system_partition_guid = GPT_EFI_SYSTEM_PARTITION_GUID;

/*-- gpt_to_partinfo -----------------------------------------------------------
 *
 *      Convert a GPT entry to a generic partition info structure.
 *
 * Parameters
 *      IN gpt_part:  pointer to the GPT partition entry
 *      IN part_id:   GPT partition number
 *      IN partition: pointer to a generic partition info structure
 *----------------------------------------------------------------------------*/
static void gpt_to_partinfo(gpt_entry *gpt_part, int part_id,
                            partition_t *partition)
{
   memset(partition, 0, sizeof (partition_t));

   partition->id = part_id;
   partition->info.start_lba = gpt_part->start_lba;
   partition->info.sectors_num = gpt_part->end_lba - gpt_part->start_lba + 1;
   if (memcmp(&basic_data_partition_guid,
              &gpt_part->type, sizeof(GUID)) == 0) {
      partition->info.type = PART_TYPE_FAT16;
   } else if (memcmp(&efi_system_partition_guid,
                     &gpt_part->type, sizeof(GUID)) == 0) {
      partition->info.type = PART_TYPE_EFI;
   } else if (memcmp(&unused_partition_guid,
                     &gpt_part->type, sizeof(GUID)) == 0) {
      partition->info.type = PART_TYPE_EMPTY;
   } else {
      /*
       * Could check for more GUIDs here, but we don't care about the exact
       * partition type if it doesn't contain a FAT filesystem.
       */
      partition->info.type = PART_TYPE_NON_FS;
   }
}

/*-- gpt_read_header -----------------------------------------------------------
 *
 *      Read the GUID Partition Table (GPT) header
 *
 * Parameters
 *      IN disk:         pointer to the disk info structure
 *      OUT gpt:         pointer to the gpt header; to be freed by caller
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int gpt_read_header(disk_t *disk, gpt_header **gpt_out)
{
   uint32_t checksum;
   gpt_header *gpt;
   int status;

   *gpt_out = NULL;

   gpt = sys_malloc(disk->bytes_per_sector);
   if (gpt == NULL) {
      return ERR_OUT_OF_RESOURCES;
   }

   status = disk_read(disk, gpt, 1, 1);
   if (status != ERR_SUCCESS) {
      sys_free(gpt);
      return status;
   }

   if (gpt->signature != GPT_SIGNATURE || gpt->myLba != 1) {
      sys_free(gpt);
      return ERR_NOT_FOUND;
   }

   checksum = gpt->headerCrc32;
   gpt->headerCrc32 = 0;
   if (crc32(0, (uint8_t *)gpt, gpt->headerSize) != checksum) {
      sys_free(gpt);
      return ERR_NOT_FOUND;
   }

   *gpt_out = gpt;
   return ERR_SUCCESS;
}

/*-- gpt_get_part_info ---------------------------------------------------------
 *
 *      Scan the GUID Partition Table (GPT) and return information for a given
 *      partition on a given disk.
 *
 * Parameters
 *      IN disk:      pointer to the disk info structure
 *      IN part_id:   GPT partition number
 *      IN partition: pointer to a generic partition info structure
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int gpt_get_part_info(disk_t *disk, int part_id, partition_t *partition)
{
   uint32_t checksum, entry_size, ptable_size, ptable_size_in_sectors;
   gpt_header *gpt;
   uint8_t *ptable;
   int status;

   status = gpt_read_header(disk, &gpt);
   if (status != ERR_SUCCESS) {
      return status;
   }

   if ((uint32_t)part_id > gpt->numberOfEntries) {
      sys_free(gpt);
      return ERR_NOT_FOUND;
   }

   entry_size = gpt->sizeOfEntry;
   ptable_size = gpt->numberOfEntries * entry_size;
   ptable_size_in_sectors = ceil(ptable_size, disk->bytes_per_sector);

   ptable = sys_malloc(ptable_size_in_sectors * disk->bytes_per_sector);
   if (ptable == NULL) {
      sys_free(gpt);
      return ERR_OUT_OF_RESOURCES;
   }

   status = disk_read(disk, ptable, gpt->entryArrayLba, ptable_size_in_sectors);
   if (status != ERR_SUCCESS) {
      sys_free(gpt);
      sys_free(ptable);
      return status;
   }

   checksum = crc32(0, ptable, ptable_size);
   if (checksum != gpt->entryArrayCrc32) {
      sys_free(gpt);
      sys_free(ptable);
      return ERR_NOT_FOUND;
   }

   gpt_to_partinfo((gpt_entry *)&ptable[(part_id - 1) * entry_size],
                   part_id, partition);

   sys_free(gpt);
   sys_free(ptable);

   return ERR_SUCCESS;
}

/*-- gpt_get_max_part ---------------------------------------------------------
 *
 *      Read the GUID Partition Table (GPT) and find the max partition number.
 *
 * Parameters
 *      IN disk:      pointer to the disk info structure
 *      OUT max:      highest partition number (1-origin); 0 if none.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int gpt_get_max_part(disk_t *disk, int *max)
{
   gpt_header *gpt;
   int status;

   status = gpt_read_header(disk, &gpt);
   if (status != ERR_SUCCESS) {
      *max = 0;
      return status;
   }

   *max = gpt->numberOfEntries;
   sys_free(gpt);

   return ERR_SUCCESS;
}

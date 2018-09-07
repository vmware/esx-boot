/*******************************************************************************
 * Copyright (c) 2008-2011 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * gpt.c -- GUID Partition Table (GPT)
 */

#include <bootlib.h>
#include <boot_services.h>
#include <string.h>
#include <zlib.h>

#define GPT_SIGNATURE   0x5452415020494645ULL
#define GPT_GUID_LEN    16

#define GPT_DATA_PARTITION_GUID                           \
   {0xA2, 0xA0, 0xD0, 0xEB, 0xE5, 0xB9, 0x33, 0x44,       \
    0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7}

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
   uint8_t diskGuid[GPT_GUID_LEN];
   uint64_t entryArrayLba;
   uint32_t numberOfEntries;
   uint32_t sizeOfEntry;
   uint32_t entryArrayCrc32;
} gpt_header;

typedef struct {
   uint8_t type[GPT_GUID_LEN];
   uint8_t guid[GPT_GUID_LEN];
   uint64_t start_lba;
   uint64_t end_lba;
   uint64_t attributes;
   uint16_t name[36];
} gpt_entry;

static const uint8_t data_partition_guid[16] = GPT_DATA_PARTITION_GUID;

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
   if (memcmp(data_partition_guid, gpt_part->type, GPT_GUID_LEN) == 0) {
      partition->info.type = PART_TYPE_GPT;
   } else {
      partition->info.type = PART_TYPE_FAT16;
   }
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

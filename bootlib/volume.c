/*******************************************************************************
 * Copyright (c) 2008-2011,2018,2021 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * volume.c -- Firmware-independent volume management
 */

#include <bootlib.h>
#include <boot_services.h>

#define ALIGN_DOWN(_offset_, _bound_) ((_offset_) - ((_offset_) % (_bound_)))

/*-- get_volume_info -----------------------------------------------------------
 *
 *      Scan a disk and return information for a given partition. Both legacy
 *      MBR partition table and GUID partition table (GPT) are supported.
 *
 * Parameters
 *      IN disk:      pointer to the disk info structure
 *      IN part_id:   MBR partition number
 *      IN partition: pointer to a generic partition info structure
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int get_volume_info(disk_t *disk, int part_id, partition_t *partition)
{
   char *mbr;
   mbr_part_t *part;
   int status;

   if (part_id <= 0) {
      return ERR_INVALID_PARAMETER;
   }

   mbr = sys_malloc(disk->bytes_per_sector);
   if (mbr == NULL) {
      return ERR_OUT_OF_RESOURCES;
   }

   status = disk_read(disk, mbr, 0, 1);
   if (status != ERR_SUCCESS) {
      sys_free(mbr);
      return status;
   }

   part = PRIMARY_PARTITION_ENTRY(mbr, 1);

   if (PART_IS_PROTECTIVE_MBR(part)) {
      status = gpt_get_part_info(disk, part_id, partition);
      if (status != ERR_NOT_FOUND) {
         sys_free(mbr);
         return status;
      }
   }

   status = mbr_get_part_info(disk, mbr, part_id, partition);
   sys_free(mbr);
   return status;
}

/*-- get_max_volume ------------------------------------------------------------
 *
 *      Scan a disk to find the highest partition number that exists.  The
 *      partition isn't necessarily valid, nor are all lower partition numbers
 *      necessarily valid, but certainly no higher partition numbers are valid.
 *      MBR partition table and GUID partition table (GPT) are supported.
 *
 * Parameters
 *      IN disk:      pointer to the disk info structure
 *      OUT max:      highest partition number (1-origin); 0 if none
 *
 * Results
 *      ERR_SUCCESS, or a generic error code.
 *
 *      Even if an error occurred, *max is set to the max partition detected
 *      before the error.
 *----------------------------------------------------------------------------*/
int get_max_volume(disk_t *disk, int *max)
{
   char *mbr;
   mbr_part_t *part;
   int status;

   *max = 0;

   mbr = sys_malloc(disk->bytes_per_sector);
   if (mbr == NULL) {
      return ERR_OUT_OF_RESOURCES;
   }

   status = disk_read(disk, mbr, 0, 1);
   if (status != ERR_SUCCESS) {
      sys_free(mbr);
      return status;
   }

   part = PRIMARY_PARTITION_ENTRY(mbr, 1);

   if (PART_IS_PROTECTIVE_MBR(part)) {
      status = gpt_get_max_part(disk, max);
   } else {
      status = mbr_get_max_part(disk, mbr, max);
   }
   sys_free(mbr);
   return status;
}

/*-- volume_read ---------------------------------------------------------------
 *
 *      Read raw bytes from a volume. All the bytes are read or an error is
 *      returned.
 *
 * Parameters
 *      IN disk:      disk to read from
 *      IN partition: volume within disk
 *      OUT dest:     output buffer
 *      IN offset:    offset of the first byte to read from
 *      IN size:      number of bytes to read
 *
 * Results
 *      ERR_SUCCESS, or a generic error code.
 *----------------------------------------------------------------------------*/
int volume_read(disk_t *disk, partition_t *partition,
                void *dest, uint64_t offset, size_t size)
{
   size_t sector, start, bytes;
   void *buffer;
   int status;

   start = ALIGN_DOWN(offset, disk->bytes_per_sector);
   bytes = (size_t)roundup64(offset + size, disk->bytes_per_sector) - start;
   sector = partition->info.start_lba + start / disk->bytes_per_sector;

   if (bytes > size) {
      buffer = sys_malloc(size);
      if (buffer == NULL) {
         return ERR_OUT_OF_RESOURCES;
      }
   } else {
      buffer = dest;
   }

   status = disk_read(disk, buffer, sector, bytes / disk->bytes_per_sector);

   if (buffer != dest) {
      if (status == ERR_SUCCESS) {
         memcpy(dest, buffer, size);
      }
      sys_free(buffer);
   }

   return status;
}

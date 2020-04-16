/*******************************************************************************
 * Copyright (c) 2008-2011,2018 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * mbr.c -- MBR partition table
 */

#include <bootlib.h>
#include <boot_services.h>
#include <string.h>

#define MBR_PART_TABLE_OFFSET   0x1be
#define MBR_SIGNATURE_OFFSET    0x1fe
#define MBR_SIGNATURE           0xaa55

#define PART_IS_EXTENDED(_part_)                         \
   ((_part_)->type == PART_TYPE_EXTENDED ||              \
    (_part_)->type == PART_TYPE_WIN_EXTENDED ||          \
    (_part_)->type == PART_TYPE_LINUX_EXTENDED)

#define IS_BOOT_RECORD(_mbr_)                            \
   (*(uint16_t *)((char *)(_mbr_) + MBR_SIGNATURE_OFFSET) == MBR_SIGNATURE)

#define MBR_PART_ENTRY(_mbr_, _entry_)                   \
   (((mbr_part_t *)((char *)(_mbr_) + MBR_PART_TABLE_OFFSET)) + ((_entry_) - 1))

/*-- mbr_to_partinfo -----------------------------------------------------------
 *
 *      Convert a MBR partition table entry to a generic partition info
 *      structure.
 *
 * Parameters
 *      IN part:      pointer to the GPT partition entry
 *      IN part_id:   MBR partition number
 *      IN extended:  pointer to the parent extended partition (if any)
 *      IN partition: pointer to a generic partition info structure
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int mbr_to_partinfo(mbr_part_t *part, int part_id, uint32_t ebr_lba,
                           mbr_part_t *extended, partition_t *partition)
{
   uint32_t lba;

   if (extended != NULL) {
      lba = part->start_lba + ebr_lba;

      if (lba < extended->start_lba ||
          lba >= extended->start_lba + extended->sectors_num ||
          lba + part->sectors_num <= extended->start_lba ||
          lba + part->sectors_num > extended->start_lba +
          extended->sectors_num) {
         return ERR_VOLUME_CORRUPTED;
      }
   } else {
      lba = part->start_lba;
   }

   memcpy(&partition->info, part, sizeof (mbr_part_t));
   partition->info.start_lba = lba;
   partition->id = part_id;

   return ERR_SUCCESS;
}

/*-- mbr_get_logical_info ------------------------------------------------------
 *
 *      Get information for a given logical partition, in a given extended
 *      partition, on a given disk.
 *
 *      NOTE: this function does not support nested extended partitions.
 *
 * Parameters
 *      IN disk:      pointer to the disk info structure
 *      IN extended:  pointer to the parent extended partition MBR entry
 *      IN part_id:   MBR logical partition number
 *      IN partition: pointer to a generic partition info structure
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int mbr_get_logical_info(disk_t *disk, mbr_part_t *extended, int part_id,
                                partition_t *partition)
{
   char *ebr;
   mbr_part_t *part;
   uint32_t next_ebr_lba;
   int partnum, status;

   ebr = sys_malloc(disk->bytes_per_sector);
   if (ebr == NULL) {
      return ERR_OUT_OF_RESOURCES;
   }

   next_ebr_lba = 0;
   partnum = 5;
   status = ERR_NOT_FOUND;

   do {
      next_ebr_lba += extended->start_lba;

      status = disk_read(disk, ebr, next_ebr_lba, 1);
      if (status != ERR_SUCCESS) {
         break;
      }

      if (!IS_BOOT_RECORD(ebr)) {
         status = ERR_VOLUME_CORRUPTED;
         break;
      }

      part = MBR_PART_ENTRY(ebr, 1);
      if (PART_IS_EXTENDED(part)) {
         status = ERR_UNSUPPORTED;
         break;
      } else {
         if (partnum == part_id) {
            status = mbr_to_partinfo(part, partnum, next_ebr_lba, extended,
                                     partition);
            break;
         }
         partnum++;
      }

      part = MBR_PART_ENTRY(ebr, 2);
      next_ebr_lba = part->start_lba;
   } while (next_ebr_lba > 0);

   sys_free(ebr);

   return status;
}

/*-- mbr_get_part_info ---------------------------------------------------------
 *
 *      Scan the MBR partition table and return information for a given
 *      partition on a given disk.
 *
 * Parameters
 *      IN disk:      pointer to the disk info structure
 *      IN mbr:       the contents of the MBR
 *      IN part_id:   MBR partition number
 *      IN partition: pointer to a generic partition info structure
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int mbr_get_part_info(disk_t *disk, char *mbr, int part_id,
                      partition_t *partition)
{
   mbr_part_t *part;
   int i;

   if (part_id < 5) {
      part = MBR_PART_ENTRY(mbr, part_id);
      return mbr_to_partinfo(part, part_id, 0, NULL, partition);
   }

   for (i = 1; i < 5; i++) {
      part = MBR_PART_ENTRY(mbr, i);
      if (PART_IS_EXTENDED(part)) {
         return mbr_get_logical_info(disk, part, part_id, partition);
      }
   }

   return ERR_NOT_FOUND;
}

/*-- mbr_get_max_part ---------------------------------------------------------
 *
 *      Scan the MBR partition table and find the max partition number.  The
 *      returned value is not necessarily a valid partition, but certainly no
 *      higher numbered partitions exist.
 *
 * Parameters
 *      IN disk:      pointer to the disk info structure
 *      IN mbr:       the contents of the MBR
 *      OUT max:      highest partition number (1-origin)
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *
 *      Even if an error occurred, *max is set to the max partition detected
 *      before the error.
 *----------------------------------------------------------------------------*/
int mbr_get_max_part(disk_t *disk, char *mbr, int *max)
{
   char *ebr;
   mbr_part_t *extended, *part;
   uint32_t next_ebr_lba;
   int i, status;

   /*
    * Assume at least standard 4 primary partitions exist.
    */
   *max = 4;

   /*
    * Look for an extended partition.  (Only one is supported.)
    */
   for (i = 1; i < 5; i++) {
      extended = MBR_PART_ENTRY(mbr, i);

      if (PART_IS_EXTENDED(extended)) {
         /*
          * Found extended partition. Count the logical partitions in it.
          */
         ebr = sys_malloc(disk->bytes_per_sector);
         if (ebr == NULL) {
            return ERR_OUT_OF_RESOURCES;
         }

         next_ebr_lba = 0;

         do {
            next_ebr_lba += extended->start_lba;

            status = disk_read(disk, ebr, next_ebr_lba, 1);
            if (status != ERR_SUCCESS) {
               break;
            }

            if (!IS_BOOT_RECORD(ebr)) {
               status = ERR_VOLUME_CORRUPTED;
               break;
            }

            part = MBR_PART_ENTRY(ebr, 1);
            if (PART_IS_EXTENDED(part)) {
               status = ERR_UNSUPPORTED;
               break;
            }

            (*max)++;

            part = MBR_PART_ENTRY(ebr, 2);
            next_ebr_lba = part->start_lba;
         } while (next_ebr_lba > 0);

         sys_free(ebr);
         break;
      }
   }

   return ERR_SUCCESS;
}

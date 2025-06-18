/*******************************************************************************
 * Copyright (c) 2008-2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * disk.c -- Disk operations
 */

#include <string.h>
#include "com32_private.h"

#pragma pack(1)
typedef struct {
   uint8_t size;           /* Packet size in bytes. Shall be 16 or greater */
   uint8_t reserved1;      /* Reserved, must be 0 */
   uint8_t count;          /* Number of blocks to transfer (Max 127) */
   uint8_t reserved2;      /* Reserved, must be 0 */
   uint16_t offset;        /* Transfer buffer offset */
   uint16_t segment;       /* Transfer buffer segment */
   uint64_t lba;           /* Starting LBA of the data to be transferred */
} device_address_packet_t;

typedef struct {
   uint16_t size;               /* Structure size, in bytes */
   uint16_t flags;              /* Feature flags */
   uint32_t cylinders;          /* Number of physical cylinders */
   uint32_t heads_per_cylinder; /* Number of physical heads per cylinder */
   uint32_t sectors_per_track;  /* Number of physical sectors per track */
   uint64_t sectors;            /* Total Number of physical sectors */
   uint16_t bytes_per_sector;   /* Number of bytes in a sector */
} drive_parameters_t;
#pragma pack()

#define LEGACY_BYTES_PER_SECTOR        512

#define LEGACY_INT13_READ_SIZE_MAX     UINT8_MAX
#define EXTENDED_INT13_READ_SIZE_MAX   127

/* Reading/writing at most 32 sectors at once seems safe on most BIOSes. */
#define SAFE_INT13_SIZE_MAX            32

#define int13(_iregs_, _oregs_)        (disk_int13((_iregs_), (_oregs_), false))
#define safe_int13(_iregs_, _oregs_)   (disk_int13((_iregs_), (_oregs_), true))

/*-- lba_to_chs ----------------------------------------------------------------
 *
 *      LBA to CHS conversion. The conversion algorithm is deduced from the
 *      following CHS to LBA conversion formula:
 *
 *      LBA = (c * HPC * SPT) + (h * SPT) + (s - 1)
 *
 *         HPC = Number of heads per cylinder
 *         SPT = Nuber of sectors per track
 *         c   = Selected cylinder number
 *         h   = Selected head number
 *         s   = Selected sector number
 *
 * Parameters
 *      IN  disk:     pointer to the disk info structure
 *      IN  lba:      logical block address
 *      OUT cylinder: selected cylinder number
 *      OUT head:     selected head number
 *      OUT sector:   selected sector number
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int lba_to_chs(const disk_t *disk, uint64_t lba, uint16_t *cylinder,
                      uint8_t *head, uint8_t *sector)
{
   uint64_t tmp, c, h, s;

   if (disk->heads_per_cylinder == 0 || disk->sectors_per_track == 0) {
      return ERR_DEVICE_ERROR;
   }

   c = lba / (disk->heads_per_cylinder * disk->sectors_per_track);
   tmp = lba % (disk->heads_per_cylinder * disk->sectors_per_track);
   h = tmp / disk->sectors_per_track;
   s = tmp % disk->sectors_per_track + 1;

   if (s > 63 || h > 255 || c > 1023) {
      return ERR_INVALID_PARAMETER;
   }

   *cylinder = (uint16_t)c;
   *head = (uint8_t)h;
   *sector = (uint8_t)s;

   return ERR_SUCCESS;
}

/*-- disk_int13 ----------------------------------------------------------------
 *
 *      Interrupt 13h wrapper.
 *
 * Parameters
 *      IN iregs: input registers
 *      IN oregs: output registers
 *      IN retry: true if the interrupt has to be retried several times
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int disk_int13(const com32sys_t *iregs, com32sys_t *oregs, bool retry)
{
   int tries;

   tries = retry ? 5 : 0;

   do {
      if (intcall_check_CF(0x13, iregs, oregs) == ERR_SUCCESS) {
         return ERR_SUCCESS;
      }
   } while (tries--);

   return ERR_DEVICE_ERROR;
}

/*-- legacy_int13_get_params ---------------------------------------------------
 *
 *      Get the disk geometry using the legacy BIOS Interrupt 13h.
 *
 * Parameters
 *      IN  drive:        BIOS drive number
 *      OUT max_cylinder: max cylinder number (total number of cylinders - 1)
 *      OUT max_head:     max head number     (number of heads per cylinder - 1)
 *      OUT max_sector:   max sector number   (number of sectors per track)
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int legacy_int13_get_params(uint8_t drive, uint8_t *max_cylinder,
                                   uint8_t *max_head, uint16_t *max_sector)
{
   com32sys_t iregs, oregs;
   int status;

   memset(&iregs, 0, sizeof (iregs));
   iregs.eax.b[1] = 0x08;
   iregs.edx.b[0] = drive;

   status = int13(&iregs, &oregs);
   if ((status != ERR_SUCCESS)) {
      return status;
   }

#ifdef BIOS_STRESS_TEST
   /*
    * According to Ralf Brown's Interrupt List, several BIOSes incorrectly
    * report high numbered drives.
    */
   if (drive > oregs.edx.b[0]) {
      return ERR_DEVICE_ERROR;
   }
#endif

   *max_cylinder = (((uint16_t)(oregs.ecx.b[0] & 0xc0)) << 2) + oregs.ecx.b[1];
   *max_head = oregs.edx.b[1];
   *max_sector = oregs.ecx.b[0] & 0x3f;

   return ERR_SUCCESS;
}

/*-- extended_int13_get_params -------------------------------------------------
 *
 *      Get the drive geometry using the Enhanced Disk Drive (EDD) interrupt 13h
 *      extension.
 *
 * Parameters
 *      IN drive:      BIOS drive number
 *      IN parameters: pointer to the EDD drive parameters info structure
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int extended_int13_get_params(uint8_t drive,
                                     drive_parameters_t *parameters)
{
   drive_parameters_t *buffer;
   com32sys_t iregs, oregs;
   farptr_t fptr;
   size_t buflen;
   int status;

   buffer = get_bounce_buffer();
   buflen = sizeof (drive_parameters_t);

   memset(buffer, 0, buflen);
   buffer->size = buflen;

   memset(&iregs, 0, sizeof (iregs));
   fptr = virtual_to_real(buffer);
   iregs.eax.b[1] = 0x48;
   iregs.edx.b[0] = drive;
   iregs.ds = fptr.real.segment;
   iregs.esi.w[0] = fptr.real.offset;

   status = int13(&iregs, &oregs);
   if ((status != ERR_SUCCESS)) {
      return status;
   }

   if (buffer->size > buflen) {
      return ERR_DEVICE_ERROR;
   }

   memcpy(parameters, buffer, buffer->size);

   if (buffer->size < buflen) {
      memset((char *)parameters + buffer->size, 0, buflen - buffer->size);
   }

   return ERR_SUCCESS;
}

/*-- extended_int13_check_extensions -------------------------------------------
 *
 *      Check whether the Enhanced Disk Drive (EDD) interrupt 13h extension is
 *      supported.
 *
 * Parameters
 *      IN  drive:   BIOS drive number
 *      OUT version: EDD version
 *      OUT flags:   EDD feature flags
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int extended_int13_check_extensions(uint8_t drive, uint8_t *version,
                                           uint16_t *flags)
{
   com32sys_t iregs, oregs;
   int status;

   memset(&iregs, 0, sizeof (iregs));
   iregs.eax.b[1] = 0x41;
   iregs.edx.b[0] = drive;
   iregs.ebx.w[0] = 0x55aa;

   status = int13(&iregs, &oregs);
   if ((status != ERR_SUCCESS) || (oregs.ebx.w[0] != 0xaa55)) {
      return ERR_DEVICE_ERROR;
   }

   *version = oregs.eax.b[1];
   *flags = oregs.ecx.w[0];

   return ERR_SUCCESS;
}

/*-- legacy_int13_rw_sectors ---------------------------------------------------
 *
 *      Read/write sectors using the legacy BIOS interrupt 13h.
 *
 * Parameters
 *      IN drive:    BIOS drive number
 *      IN cylinder: the cylinder to transfer
 *      IN head:     the head to transfer
 *      IN sector:   the sector to transfer from/to
 *      IN read:     true = read, false = write
 *      IN buffer:   pointer to the data buffer
 *      IN count:    number of sectors to transfer
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int legacy_int13_rw_sectors(uint8_t drive, uint16_t cylinder,
                                   uint8_t head, uint8_t sector, bool read,
                                   void *buffer, uint8_t count)
{
   com32sys_t iregs, oregs;
   farptr_t fptr;
   void *buf;
   int status;

   if (count == 0) {
      return ERR_SUCCESS;
   }

   if (sector == 0 || sector > 63 || cylinder > 1023 ||
       ((size_t)count * LEGACY_BYTES_PER_SECTOR > get_bounce_buffer_size())) {
      return ERR_INVALID_PARAMETER;
   }

   buf = get_bounce_buffer();

   if (!read) {
      memcpy(buf, buffer, count * LEGACY_BYTES_PER_SECTOR);
   }

   memset(&iregs, 0, sizeof (iregs));
   fptr = virtual_to_real(buf);
   iregs.eax.b[1] = read ? 0x02 : 0x03;
   iregs.eax.b[0] = count;
   iregs.ecx.b[1] = cylinder & 0xff;
   iregs.ecx.b[0] = ((cylinder & 0x300) >> 2) + sector;
   iregs.edx.b[1] = head;
   iregs.edx.b[0] = drive;
   iregs.ebx.w[0] = fptr.real.offset;
   iregs.es = fptr.real.segment;

   status = safe_int13(&iregs, &oregs);
   if (status != ERR_SUCCESS) {
      Log(LOG_DEBUG, "legacy_int13_rw_sectors retcode=0x%x written=0x%x",
          oregs.eax.b[1], oregs.eax.b[0]);
      return status;
   }

   if (read) {
      memcpy(buffer, buf, count * LEGACY_BYTES_PER_SECTOR);
   }

   return ERR_SUCCESS;
}

/*-- extended_int13_rw_sectors -------------------------------------------------
 *
 *      Read/Write sectors using the Enhanced Disk Drive bios extensions.
 *
 * Parameters
 *      IN drive:            BIOS drive number
 *      IN bytes_per_sector: sector size, in bytes
 *      IN lba:              first sector LBA to transfer
 *      IN read:             true = read, false = write
 *      IN buffer:           pointer to the data buffer
 *      IN count:            number of sectors to transfer
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int extended_int13_rw_sectors(uint8_t drive, uint16_t bytes_per_sector,
                                     uint64_t lba, bool read, void *buffer,
                                     uint8_t count)
{
   device_address_packet_t *dap;
   com32sys_t iregs, oregs;
   farptr_t fptr;
   int status;
   void *buf;

   if (count == 0) {
      return ERR_SUCCESS;
   }

   if (count > EXTENDED_INT13_READ_SIZE_MAX ||
       ((size_t)count + 1) * bytes_per_sector > get_bounce_buffer_size()) {
      return ERR_INVALID_PARAMETER;
   }

   dap = get_bounce_buffer();
   buf = (char *)dap + bytes_per_sector;

   if (!read) {
      memcpy(buf, buffer, count * bytes_per_sector);
   }

   memset(dap, 0, sizeof (device_address_packet_t));
   fptr = virtual_to_real(buf);
   dap->size = sizeof (device_address_packet_t);
   dap->count = count;
   dap->offset = fptr.real.offset;
   dap->segment = fptr.real.segment;
   dap->lba = lba;

   memset(&iregs, 0, sizeof (iregs));
   fptr = virtual_to_real(dap);
   iregs.eax.b[1] = read ? 0x42 : 0x43;
   iregs.edx.b[0] = drive;
   iregs.esi.w[0] = fptr.real.offset;
   iregs.ds = fptr.real.segment;

   status = safe_int13(&iregs, &oregs);
   if (status != ERR_SUCCESS) {
      Log(LOG_DEBUG, "extended_int13_rw_sectors retcode=0x%x", oregs.eax.b[1]);
      return status;
   }

   if (read) {
      memcpy(buffer, buf, count * bytes_per_sector);
   }

   return ERR_SUCCESS;
}

/*-- get_max_numsectors --------------------------------------------------------
 *
 *      Truncate the given number of sectors to the maximum that it is safe to
 *      read/write at once.
 *
 * Parameters
 *      IN disk:    pointer to the disk info structure
 *      IN sectors: number of sectors to be accessed
 *
 * Results
 *      The maximum number of sector that can be accessed at once.
 *----------------------------------------------------------------------------*/
static INLINE size_t get_max_numsectors(const disk_t *disk, size_t sectors)
{
   if (sectors == 0) {
      return 0;
   }

   sectors = MIN(sectors, SAFE_INT13_SIZE_MAX);
   sectors = MIN(sectors, get_bounce_buffer_size() / disk->bytes_per_sector);

   return MAX(sectors, 1);
}

/*-- disk_read -----------------------------------------------------------------
 *
 *      Read sectors from a disk. All sectors are read, or an error is returned.
 *
 * Parameters
 *      IN disk:   pointer to the disk info structure
 *      IN buffer: pointer to the output buffer
 *      IN lba:    first sector LBA to read from
 *      IN count:  number of sectors to read
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int disk_read(const disk_t *disk, void *buffer, uint64_t lba, size_t count)
{
   char *current_buffer;
   size_t offset, numsectors;
   uint8_t head, sector;
   uint16_t cylinder;
   int status;

   for (offset = 0; offset < count; offset += numsectors) {
      numsectors = get_max_numsectors(disk, count - offset);
      current_buffer = (char*)buffer + offset * disk->bytes_per_sector;

      if (disk->use_edd) {
         status = extended_int13_rw_sectors(disk->firmware_id,
                                            disk->bytes_per_sector,
                                            lba + offset, true, current_buffer,
                                            numsectors);
      } else {
         status = lba_to_chs(disk, lba + offset, &cylinder, &head, &sector);
         if (status == ERR_SUCCESS) {
            status = legacy_int13_rw_sectors(disk->firmware_id, cylinder, head,
                                             sector, true, current_buffer,
                                             numsectors);
         }
      }

      if (status != ERR_SUCCESS) {
         return status;
      }
   }

   return ERR_SUCCESS;
}

/*-- disk_write ----------------------------------------------------------------
 *
 *      Write sectors to a disk. All sectors are written, or an error is
 *      returned.
 *
 * Parameters
 *      IN disk:   pointer to the disk info structure
 *      IN buffer: pointer to the buffer to read from
 *      IN lba:    first sector LBA to write to
 *      IN count:  number of sectors to write
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int disk_write(const disk_t *disk, void *buffer, uint64_t lba, size_t count)
{
   char *current_buffer;
   size_t offset, numsectors;
   uint8_t head, sector;
   uint16_t cylinder;
   int status;
   void *buf;

   if (count == 0) {
      return ERR_SUCCESS;
   }

   for (offset = 0; offset < count; offset += numsectors) {
      numsectors = get_max_numsectors(disk, count - offset);
      current_buffer = (char*)buffer + offset * disk->bytes_per_sector;

      if (disk->use_edd) {
         status = extended_int13_rw_sectors(disk->firmware_id,
                                            disk->bytes_per_sector,
                                            lba + offset, false, current_buffer,
                                            numsectors);
      } else {
         status = lba_to_chs(disk, lba + offset, &cylinder, &head, &sector);
         if (status == ERR_SUCCESS) {
            status = legacy_int13_rw_sectors(disk->firmware_id, cylinder, head,
                                             sector, false, current_buffer,
                                             numsectors);
         }
      }

      if (status != ERR_SUCCESS) {
         return status;
      }
   }

   buf = malloc(count * disk->bytes_per_sector);
   if (buf == NULL) {
      return ERR_OUT_OF_RESOURCES;
   }

   status = disk_read(disk, buf, lba, count);
   if (status != ERR_SUCCESS) {
      Log(LOG_DEBUG, "disk_write: readback returned %d\n", status);
      free(buf);
      return status;
   }

   if (memcmp(buffer, buf, count * disk->bytes_per_sector) != 0) {
      Log(LOG_DEBUG, "disk_write: readback value doesn't match\n");
      free(buf);
      return ERR_DEVICE_ERROR;
   }

   free(buf);

   return ERR_SUCCESS;
}

/*-- get_disk_info -------------------------------------------------------------
 *
 *      Get disk information.
 *
 * Parameters
 *      IN drive: BIOS drive number
 *      IN disk:  pointer to the disk info structure to be filled
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int get_disk_info(uint8_t drive, disk_t *disk)
{
   drive_parameters_t params;
   uint8_t version, c, h;
   uint16_t flags, s, bytes_per_sector;
   bool edd_present;
   int status;

   edd_present = false;

   status = extended_int13_check_extensions(drive, &version, &flags);
   if (status == ERR_SUCCESS) {
      status = extended_int13_get_params(drive, &params);
      if (status == ERR_SUCCESS) {
         c = params.cylinders;
         h = params.heads_per_cylinder;
         s = params.sectors_per_track;
         bytes_per_sector = params.bytes_per_sector;
         edd_present = true;
      }
   }

   if (!edd_present) {
      status = legacy_int13_get_params(drive, &c, &h, &s);
      if (status != ERR_SUCCESS) {
         return status;
      }

      bytes_per_sector = LEGACY_BYTES_PER_SECTOR;
      c++;
      h++;
   }

   memset(disk, 0, sizeof (disk_t));
   disk->firmware_id = drive;
   disk->use_edd = edd_present;
   disk->cylinders = c;
   disk->heads_per_cylinder = h;
   disk->sectors_per_track = s;
   disk->bytes_per_sector = bytes_per_sector;

   return ERR_SUCCESS;
}

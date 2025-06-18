/*******************************************************************************
 * Copyright (c) 2008-2011 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * disk.c -- Raw disk access
 */

#include <boot_services.h>

#include "efi_private.h"

/*-- get_boot_disk -------------------------------------------------------------
 *
 *      Get the disk info structure for the boot disk.
 *
 * Parameters
 *      OUT disk: pointer to the disk info structure
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int get_boot_disk(disk_t *disk)
{
   EFI_HANDLE Volume;
   EFI_BLOCK_IO *Block;
   EFI_STATUS Status;

   Status = get_boot_device(&Volume);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   Status = get_protocol_interface(Volume, &BlockIoProto, (void **)&Block);
   if (EFI_ERROR(Status)) {
      return error_efi_to_generic(Status);
   }

   EFI_ASSERT_FIRMWARE(Block->Media != NULL);

   memset(disk, 0, sizeof (disk_t));
   disk->firmware_id = (uintptr_t)Block;
   disk->use_edd = TRUE;
   disk->cylinders = 0;
   disk->heads_per_cylinder = 0;
   disk->sectors_per_track = 0;
   disk->bytes_per_sector = Block->Media->BlockSize;

   return error_efi_to_generic(EFI_SUCCESS);
}

/*-- disk_read -----------------------------------------------------------------
 *
 *      Read raw blocks from a disk using the Block I/O protocol. All blocks are
 *      read, or an error is returned.
 *
 * Parameters
 *      IN disk:  pointer to the disk info structure
 *      IN buf:   pointer to the output buffer
 *      IN lba:   LBA of the first block to read
 *      IN count: number of blocks to read
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int disk_read(const disk_t *disk, void *buf, uint64_t lba, size_t count)
{
   EFI_BLOCK_IO *Block = (EFI_BLOCK_IO *)disk->firmware_id;
   EFI_STATUS Status;

   if (count == 0) {
      return error_efi_to_generic(EFI_SUCCESS);
   }

   EFI_ASSERT_PARAM(buf != NULL);

   EFI_ASSERT_FIRMWARE(Block->ReadBlocks != NULL);
   EFI_ASSERT_FIRMWARE(Block->Media != NULL);

   Status = Block->ReadBlocks(Block, Block->Media->MediaId, lba,
                              count * disk->bytes_per_sector, buf);

   return error_efi_to_generic(Status);
}

/*-- disk_write ----------------------------------------------------------------
 *
 *      Write raw blocks to a disk using the Block I/O protocol. All blocks are
 *      written, or an error is returned.
 *
 * Parameters
 *      IN disk:  pointer to the disk info structure
 *      IN buf:   pointer to the input buffer
 *      IN lba:   LBA of the first block to write
 *      IN count: number of blocks to write
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int disk_write(const disk_t *disk, void *buf, uint64_t lba, size_t count)
{
   EFI_BLOCK_IO *Block = (EFI_BLOCK_IO *)disk->firmware_id;
   EFI_STATUS Status;

   if (count == 0) {
      return error_efi_to_generic(EFI_SUCCESS);
   }

   EFI_ASSERT_PARAM(buf != NULL);

   EFI_ASSERT_FIRMWARE(Block->ReadBlocks != NULL);
   EFI_ASSERT_FIRMWARE(Block->Media != NULL);

   Status = Block->WriteBlocks(Block, Block->Media->MediaId, lba,
                               count * disk->bytes_per_sector, buf);

   return error_efi_to_generic(Status);
}

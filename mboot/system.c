/*******************************************************************************
 * Copyright (c) 2008-2017 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * system.c -- Various system routines.
 */

#include <cpu.h>
#include <boot_services.h>
#include "system_int.h"
#include "mboot.h"

int dump_firmware_info(void)
{
   firmware_t firmware;
   int status;

   status = get_firmware_info(&firmware);
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "Firmware detection failure.\n");
      return status;
   }

   switch (firmware.interface) {
      case FIRMWARE_INTERFACE_EFI:
         Log(LOG_DEBUG, "%s v%u.%u (%s, Rev.%u)\n",
             (firmware.version.efi.major > 1) ? "UEFI" : "EFI",
             (uint32_t)firmware.version.efi.major,
             (uint32_t)firmware.version.efi.minor,
             ((firmware.vendor != NULL) ? firmware.vendor : "Unknown vendor"),
             firmware.revision);
         break;
      case FIRMWARE_INTERFACE_COM32:
         Log(LOG_DEBUG, "COM32 v%u.%u (%s)\n",
             (uint32_t)firmware.version.com32.major,
             (uint32_t)firmware.version.com32.minor,
             ((firmware.vendor != NULL) ?
              firmware.vendor : "Unknown derivative"));
         break;
      default:
         Log(LOG_WARNING, "Unknown firmware\n");
   }

   sys_free(firmware.vendor);

   return ERR_SUCCESS;
}

/*-- reserve_sysmem ------------------------------------------------------------
 *
 *      Blacklist system memory so it will not be used later for run-time
 *      relocations.
 *
 * Parameters
 *      IN name: memory region description string
 *      IN addr: pointer to the memory region to be blacklisted
 *      IN size: memory region size
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static INLINE int reserve_sysmem(const char *name, void *addr, uint64_t size)
{
   Log(LOG_DEBUG, "%s found @ %p (%"PRIu64" bytes)\n", name, addr, size);

   return blacklist_runtime_mem(PTR_TO_UINT64(addr), size);
}

/*-- reserve_smbios_ranges -----------------------------------------------------
 *
 *      Register the SMBIOS memory.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int reserve_smbios_ranges(void *eps_start, size_t eps_length,
                                 void *table_start, size_t table_length)
{
   int status;

   if (!is_valid_firmware_table(eps_start, eps_length)) {
      return reserve_sysmem("SMBIOS: invalid entry point structure",
                            eps_start, eps_length);
   }

   status = reserve_sysmem("SMBIOS: entry point structure", eps_start,
                           eps_length);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = reserve_sysmem("SMBIOS: table", table_start, table_length);
   if (status != ERR_SUCCESS) {
      return status;
   }

   return ERR_SUCCESS;
}

/*-- scan_smbios_memory --------------------------------------------------------
 *
 *      Register the SMBIOS memory.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int scan_smbios_memory(void)
{
   void *eps_start;
   size_t eps_length;
   void *table_start;
   size_t table_length;
   int status;

   status = smbios_get_info(&eps_start, &eps_length, &table_start, &table_length);
   if (status == ERR_SUCCESS && eps_length != 0) {
      status = reserve_smbios_ranges(eps_start, eps_length, table_start,
                                     table_length);
      if (status != ERR_SUCCESS) {
         Log(LOG_ERR, "Failed to reserve legacy 32-bit SMBIOS ranges\n");
         return status;
      }
   }

   status = smbios_get_v3_info(&eps_start, &eps_length, &table_start, &table_length);
   if (status == ERR_SUCCESS && eps_length != 0) {
      status = reserve_smbios_ranges(eps_start, eps_length, table_start,
                                     table_length);
      if (status != ERR_SUCCESS) {
         Log(LOG_ERR, "Failed to reserve v3 64-bit SMBIOS ranges\n");
         return status;
      }
   }

   /* No SMBIOS found, do nothing. */
   return ERR_SUCCESS;
}

/*-- system_blacklist_memory ---------------------------------------------------
 *
 *      List all the memory ranges that may not be used by the bootloader.
 *
 * Parameters
 *      IN mmap:  pointer to the system memory map
 *      IN count: number of entries in the memory map
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int system_blacklist_memory(e820_range_t *mmap, size_t count)
{
   int status;

   status = system_arch_blacklist_memory();
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = scan_smbios_memory();
   if (status != ERR_SUCCESS) {
      return status;
   }
   status = e820_to_blacklist(mmap, count);
   if (status != ERR_SUCCESS) {
      return status;
   }

   return ERR_SUCCESS;
}

/*-- firmware_shutdown ---------------------------------------------------------
 *
 *     Shutdown the boot services:
 *       - Get the run-time E820 memory map (and request some extra memory for
 *         converting it later to the possibly bigger Mu(l)tiboot format).
 *
 *       - Record some EFI-specific information if possible.
 *
 *       - Claim that we no longer need the firmware boot services.
 *
 *       - Disable hardware interrupts. Since firmware services have been shut
 *         down, it is no longer necessary to run firmware interrupt handlers.
 *         After this function is called, it is safe to clobber the IDT and GDT.
 *
 * Parameters
 *      OUT mmap:     pointer to the freshly allocated memory map
 *      OUT count:    number of entries in the memory map
 *      OUT efi_info: EFI information if available
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *
 * Side Effects
 *      No call to the boot services may be done after a call to this function.
 *----------------------------------------------------------------------------*/
int firmware_shutdown(e820_range_t **mmap, size_t *count, efi_info_t *efi_info)
{
   size_t desc_extra_mem;
   int status;

   if (boot_mmap_desc_size() > sizeof (e820_range_t)) {
      desc_extra_mem = boot_mmap_desc_size() - sizeof (e820_range_t);
   } else {
      desc_extra_mem = 0;
   }

   status = exit_boot_services(desc_extra_mem, mmap, count, efi_info);
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "Failed to shutdown the boot services.\n");
      return status;
   }

   CLI();

   Log(LOG_DEBUG, "Scanning system tables...");

   e820_mmap_merge(*mmap, count);

   status = system_blacklist_memory(*mmap, *count);
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "Error scanning system memory.\n");
      return status;
   }

   return ERR_SUCCESS;
}

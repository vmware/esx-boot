/*******************************************************************************
 * Copyright (c) 2008-2011,2015-2016 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * multiboot.c -- Multiboot support
 */

#include <string.h>
#include <stdio.h>
#include <multiboot.h>
#include <stdbool.h>
#include <e820.h>
#include <boot_services.h>

#include "mboot.h"

/* We don't support these Multiboot Header flags */
#define MBH_FLAGS_UNSUPPORTED 0x0000FFF8

#define MB_MMAP_NEXT_DESC(_current_)                                    \
   ((MultiBoot_MemMap *)((char *)(_current_) + mb_mmap_desc_size()))

static MultiBoot_Info mb_info;      /* The Multiboot Info structure */
static MultiBoot_Module *mb_mods;   /* Pointer to the Multiboot modules array */
static MultiBoot_MemMap *mb_mmap;   /* Pointer to the Multiboot memory map */
static vbe_info_t vbe;              /* VBE information */
static char **cmdlines;

static void mb_mmap_sanity_check(MultiBoot_MemMap *mmap, size_t size)
{
   uint64_t max_base, max_limit, base, len, limit;
   bool error, overlap;
   const char *msg;
   size_t count, i;
   MultiBoot_MemMap *desc;

   overlap = false;
   count = size / mb_mmap_desc_size();

   if (size % mb_mmap_desc_size() != 0) {
      error = true;
      Log(LOG_ERR, "Invalid Multiboot MemMap size.\n");
   } else if (count < 1) {
      error = true;
      Log(LOG_ERR, "Multiboot MemMap is empty.\n");
   } else {
      error = false;
      max_base = 0;
      max_limit = 0;
      desc = mmap;

      for (i = 0; i < count; i++) {
         msg = NULL;
         base = ((uint64_t)desc->highAddr << 32) + desc->lowAddr;
         len = ((uint64_t)desc->highLen << 32) + desc->lowLen;;
         limit = base + len - 1;

         if (desc->size != mb_mmap_desc_size() - sizeof (desc->size)) {
            msg = "Invalid Multiboot MemMap entry size";
         } else if (!(i + 1 == count && base + len == 0) &&
                    (base + len < base)) {
            msg = "Multiboot MemMap descriptor limit overflow";
         } else if (base < max_base) {
            msg = "Multiboot MemMap is not sorted";
         }

         if (len > 0 && limit < max_limit) {
            overlap = true;
         }

         if (msg != NULL) {
            error = true;
            Log(LOG_ERR, "mmap[%zu]: %"PRIx64" - %"PRIx64" type %u: %s.\n",
                i, base, limit, desc->type, msg);
         }

         max_base = base;
         max_limit = limit;
         desc = MB_MMAP_NEXT_DESC(desc);
      }
   }

   if (overlap || error) {
      desc = mmap;
      for (i = 0; i < count; i++) {
         base = E820_BASE(desc);
         limit = E820_BASE(desc) + E820_LENGTH(desc) - 1;
         Log(LOG_DEBUG, "mmap[%zu]: %"PRIx64" - %"PRIx64" type %u\n",
             i, base, limit, desc->type);
         desc = MB_MMAP_NEXT_DESC(desc);
      }

      if (overlap) {
         Log(LOG_WARNING, "Multiboot MemMap contains overlapping ranges.\n");
      }

      if (error) {
         Log(LOG_ERR, "Multiboot MemMap is corrupted.\n");
         while (1);
      }
   }
}

/*-- mb_mmap_desc_size ---------------------------------------------------------
 *
 *      Return the size of a Multiboot memory map entry. The returned size
 *      includes the 4 bytes of the 'size' descriptor field.
 *
 * Results
 *      the descriptor size, in bytes.
 *----------------------------------------------------------------------------*/
size_t mb_mmap_desc_size(void)
{
   return boot.no_mem_attr ? MBI_MMAP_ENTRY_MIN_SIZE :
      MBI_MMAP_ENTRY_MIN_SIZE + sizeof (uint32_t);
}

/*-- mb_set_mmap_entry ---------------------------------------------------------
 *
 *      Setup a Multiboot memory map entry.
 *
 * Parameters
 *      IN desc:       pointer to the memory map descriptor
 *      IN base:       memory region starting address
 *      IN len:        memory region size, in bytes
 *      IN type:       memory region e820 type
 *      IN attributes: e820 memory attributes
 *----------------------------------------------------------------------------*/
static void mb_set_mmap_entry(MultiBoot_MemMap *desc, uint64_t base,
                              uint64_t len, uint32_t type, uint32_t attributes)
{
   desc->size = mb_mmap_desc_size() - sizeof (desc->size);
   desc->lowAddr = lowhalf64(base);
   desc->highAddr = highhalf64(base);
   desc->lowLen = lowhalf64(len);
   desc->highLen = highhalf64(len);
   desc->type = type;

   if (!boot.no_mem_attr) {
      *(uint32_t *)MBI_MMAP_EXTENDED_ATTR(desc) = attributes;
   }
}

/*-- check_multiboot_kernel ----------------------------------------------------
 *
 *      Check whether the given buffer contains a valid Multiboot kernel.
 *
 * Parameters
 *      IN kbuf:  pointer to the kernel buffer
 *      IN ksize: kernel buffer size, in bytes
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int check_multiboot_kernel(void *kbuf, size_t ksize)
{
   MultiBoot_Header *mbh;
   int status;
   size_t i;

   if (kbuf == NULL || ksize == 0) {
      return ERR_INVALID_PARAMETER;
   }

   if (ksize < sizeof (MultiBoot_Header)) {
      Log(LOG_ERR, "Kernel is too small.\n");
      return ERR_BAD_TYPE;
   }

   for (i = 0; i < MBH_ALIGNMENT; i++) {
      mbh = mbh_scan((char *)kbuf + i, ksize - i);
      if (mbh != NULL) {
         break;
      }
   }

   if (mbh == NULL) {
      Log(LOG_DEBUG, "Multiboot header is not found.\n");
      return ERR_BAD_TYPE;
   }

   if (i > 0) {
      Log(LOG_ERR, "Multiboot header is not %u-bytes aligned.\n",
          MBH_ALIGNMENT);
      return ERR_BAD_TYPE;
   }

   if (mbh->flags & MBH_FLAGS_UNSUPPORTED) {
      Log(LOG_ERR, "Multiboot header contains unsupported flags.\n");
      return ERR_BAD_TYPE;
   }

   if ((mbh->flags & MBH_FLAG_AOUT_KLUDGE) != 0) {
      Log(LOG_ERR, "Unsupported Multiboot binary format.\n");
      return ERR_BAD_TYPE;
   }

   status = elf_check_headers(kbuf, ksize, NULL);
   if (IS_WARNING(status)) {
      Log(LOG_WARNING, "Funny-looking ELF.\n");
   } else if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "Invalid ELF binary.\n");
      return ERR_BAD_TYPE;
   }

   boot.efi_info.rts_size = 0;
   boot.efi_info.rts_vaddr = 0;
   boot.efi_info.caps |= EFI_RTS_CAP_RTS_SIMPLE;
   if ((mbh->flags & MBH_FLAG_EFI_RTS_OLD) != 0) {
      boot.efi_info.rts_vaddr = mbh->rts_vaddr;
      /*
       * The old way stuffed RTS into DirectMap,
       * and this is the implicit size of that region.
       */
      boot.efi_info.rts_size = (64UL*1024*1024*1024*1024);
   }

   if ((mbh->flags & MBH_FLAG_EFI_RTS_NEW) != 0) {
      boot.efi_info.rts_vaddr = mbh->rts_vaddr;
      boot.efi_info.rts_size = mbh->rts_size;
      boot.efi_info.caps |= EFI_RTS_CAP_RTS_SPARSE |
         EFI_RTS_CAP_RTS_COMPACT |
         EFI_RTS_CAP_RTS_CONTIG;
   }

   return ERR_SUCCESS;
}

/*-- mbi_set_memory_info -------------------------------------------------------
 *
 *      Set memory-related fields in the MBI.
 *      This includes the memory map and the lower/upper memory limits.
 *
 *      Lower memory is the amount of free memory starting at address 0.
 *      Upper memory is the amount of free memory starting at 1-Mb.
 *
 *      NOTE: This function assumes that the given memory map is sorted by
 *            increasing descriptors base addresses.
 *
 * Parameters
 *      IN mbi:   pointer to the Multiboot Info structure
 *      IN mmap:  pointer to the Multiboot memory map
 *      IN count: number of entries in the memory map
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int mbi_set_memory_info(MultiBoot_Info *mbi, MultiBoot_MemMap *mmap,
                               size_t count)
{
   uint64_t base, len, mem_lower, mem_upper;
   run_addr_t addr;
   int status;
   size_t i;

   status = runtime_addr(mmap, &addr);
   if (status != ERR_SUCCESS) {
      return status;
   }

   mbi_set_mmap(mbi, (uint32_t)addr, (uint32_t)(count * mb_mmap_desc_size()));

   mem_lower = 0;
   mem_upper = 0;

   for (i = 0; i < count; i++) {
      base = E820_BASE(mmap);
      len = E820_LENGTH(mmap);

      if (base > MBI_UPPER_MEM_START) {
         break;
      }

      if (len > 0 && mmap->type == E820_TYPE_AVAILABLE) {
         if (base == 0) {
            mem_lower = MIN(len, MBI_LOWER_MEM_END);
         }

         if (base + len > MBI_UPPER_MEM_START) {
            mem_upper = base + len - MBI_UPPER_MEM_START;
            break;
         }
      }

      mmap = MB_MMAP_NEXT_DESC(mmap);
   }

   mbi_set_mem(mbi, (uint32_t)mem_lower / 1024, (uint32_t)mem_upper / 1024);

   return ERR_SUCCESS;
}

/*-- mbi_set_modules_info ------------------------------------------------------
 *
 *      Set modules-related fields in the MBI. This includes the modules command
 *      lines and locations, as well as the modules table itself.
 *
 * Parameters
 *      IN mbi:        pointer to the Multiboot Info structure
 *      IN modinfo:    pointer to the Multiboot Module info structure
 *      IN mods:       pointer to the module info structure
 *      IN mods_count: number of modules
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int mbi_set_modules_info(MultiBoot_Info *mbi, MultiBoot_Module *modinfo,
                                module_t *mods, unsigned int mods_count)
{
   run_addr_t addr, cmdline;
   unsigned int i;
   int status;

   for (i = 0; i < mods_count; i++) {
      status = runtime_addr(cmdlines[i + 1], &cmdline);
      if (status != ERR_SUCCESS) {
         return status;
      }
      if (mods[i].size > 0) {
         status = runtime_addr(mods[i].addr, &addr);
         if (status != ERR_SUCCESS) {
            return status;
         }
      } else {
         addr = 0;
      }
      mbi_set_module(&modinfo[i], (uint32_t)cmdline, (uint32_t)addr,
                     (uint32_t)mods[i].size);
   }

   status = runtime_addr(modinfo, &addr);

   if (status == ERR_SUCCESS) {
      mbi_set_mods_table(mbi, (uint32_t)addr, mods_count);
   }

   return status;
}

/*-- mbi_set_kernel_info -------------------------------------------------------
 *
 *      Set kernel-related fields in the MBI.
 *
 *      This is just the kernel command line today.
 *
 * Parameters
 *      IN mbi:    pointer to the Multiboot Info structure
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int mbi_set_kernel_info(MultiBoot_Info *mbi)
{
   run_addr_t addr;
   int status;

   status = runtime_addr(cmdlines[0], &addr);
   if (status != ERR_SUCCESS) {
      return status;
   }

   mbi_set_cmdline(mbi, (uint32_t)addr);

   return status;
}

/*-- mbi_set_vbe_info ----------------------------------------------------------
 *
 *      Set VBE-related fields in the MBI.
 *
 * Parameters
 *      IN mbi:       pointer to the Multiboot Info structure
 *      IN vbe_info:  pointer to the VBE controller info structure
 *      IN vbe_modes: pointer to the supported VBE modes list
 *      IN mode_info: pointer to the current VBE mode info structure
 *      IN mode_id:   current VBE mode identifier
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int mbi_set_vbe_info(MultiBoot_Info *mbi, vbe_t *vbe_info,
                            vbe_mode_id_t *vbe_modes, vbe_mode_t *mode_info,
                            vbe_mode_id_t mode_id)
{
   run_addr_t info, mode, modes_list;
   int status;

   status = runtime_addr(vbe_modes, &modes_list);
   if (status != ERR_SUCCESS) {
      return status;
   }
   status = runtime_addr(vbe_info, &info);
   if (status != ERR_SUCCESS) {
      return status;
   }
   status = runtime_addr(mode_info, &mode);
   if (status != ERR_SUCCESS) {
      return status;
   }

   vbe_info->VideoModePtr = (uint32_t)modes_list;
   mbi_set_vbe(mbi, (uint32_t)info, (uint32_t)mode, mode_id);

   return ERR_SUCCESS;
}

/*-- e820_to_multiboot ---------------------------------------------------------
 *
 *      Convert a E820 memory map to the Multiboot memory map format. Both E820
 *      and Multiboot buffer may overlap as long as the destination buffer is
 *      large enough for holding the converted memory map.
 *
 * Parameters
 *      IN e820:      E820 memory map
 *      IN/OUT count: number of entries in the E820 memory map
 *      IN buffer:    buffer to write the Multiboot memory map into
 *      IN buflen:    output buffer size, in bytes
 *      OUT buflen:   Multiboot memory map size, in bytes
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *
 * Side effects
 *      The E820 map is modified in place, with E820_TYPE_BOOTLOADER and
 *      E820_TYPE_BLACKLISTED_FIRMWARE_BS entries changed to the
 *      E820_TYPE_AVAILABLE type and coalesced (merged). The modified
 *      E820 map is used to generate the converted memory map.
 *----------------------------------------------------------------------------*/
static int e820_to_multiboot(e820_range_t *e820, size_t *count, void *buffer,
                             size_t *buflen)
{
   MultiBoot_MemMap *mb;
   e820_range_t *range = e820;
   e820_range_t *last = range + *count;
   size_t e820_size, mb_size, n;
   uint64_t base, length, end;
   uint32_t attributes;

   if (*count == 0) {
      return ERR_INVALID_PARAMETER;
   }

   while (range < last) {
      if (range->type == E820_TYPE_BOOTLOADER ||
          range->type == E820_TYPE_BLACKLISTED_FIRMWARE_BS) {
         range->type = E820_TYPE_AVAILABLE;
      }
      range++;
   }

   Log(LOG_DEBUG, "E820 count before final merging: %zu\n", *count);
   e820_mmap_merge(e820, count);
   Log(LOG_DEBUG, "E820 count after final merging: %zu\n", *count);

   mb_size = *count * mb_mmap_desc_size();
   if (mb_size > *buflen) {
      return ERR_BUFFER_TOO_SMALL;
   }

   /* Ensure we do not overwrite our source when writing to the destination */
   mb = buffer;
   e820_size = *count * sizeof (e820_range_t);
   e820 = memmove((char *)mb + mb_size - e820_size, e820, e820_size);

   n = 0;
   length = 0;

   /* More initialization to make GCC <= 4.1 happy */
   attributes = 0;
   end = 0;

   while ((*count)-- > 0) {
      if (n > 0 &&
          mb->type == E820_TYPE_AVAILABLE && mb->type == e820->type &&
          attributes == e820->attributes && E820_BASE(e820) == end + 1) {
         length += E820_LENGTH(e820);
         end += E820_LENGTH(e820);
         mb->highLen = highhalf64(length);
         mb->lowLen = lowhalf64(length);
      } else {
         if (n > 0) {
            mb = MB_MMAP_NEXT_DESC(mb);
         }

         base = E820_BASE(e820);
         length = E820_LENGTH(e820);
         end = base + length - 1;
         attributes = e820->attributes;

         mb_set_mmap_entry(mb, base, length, e820->type, attributes);
         n++;
      }
      e820++;
   }

   *buflen = n * mb_mmap_desc_size();

   return ERR_SUCCESS;
}

/*-- multiboot_set_runtime_pointers --------------------------------------------
 *
 *      1) Destructively convert boot.mmap from e820 format to multiboot format.
 *
 *      2) Setup the Multiboot Info structure internal pointers to their
 *      run-time (relocated) values.
 *
 * Parameters
 *      OUT run_mbi: address of the relocated Multiboot Info structure.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int multiboot_set_runtime_pointers(run_addr_t *run_mbi)
{
   size_t count, mmap_size;
   run_addr_t addr;
   int status;

   Log(LOG_DEBUG, "Converting e820 map to Multiboot format...\n");

   count = boot.mmap_count;
   mb_mmap = (MultiBoot_MemMap *)boot.mmap;
   mmap_size = count * mb_mmap_desc_size();

   status = e820_to_multiboot(boot.mmap, &count, mb_mmap, &mmap_size);
   boot.mmap = NULL;  // no longer a valid e820 map
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "Multiboot memory map error.\n");
      return status;
   }

   mb_info.mmap_length = (uint32_t)mmap_size;

   mb_mmap_sanity_check(mb_mmap, mb_info.mmap_length);

   Log(LOG_DEBUG, "Setting up Multiboot runtime references...\n");

   status = mbi_set_memory_info(&mb_info, mb_mmap,
                                mb_info.mmap_length / mb_mmap_desc_size());
   if (status != ERR_SUCCESS) {
      return status;
   }
   status = mbi_set_modules_info(&mb_info, mb_mods, &boot.modules[1],
                                 boot.modules_nr - 1);
   if (status != ERR_SUCCESS) {
      return status;
   }
   status = mbi_set_kernel_info(&mb_info);
   if (status != ERR_SUCCESS) {
      return status;
   }

   if (vbe.modes_list != NULL) {
      status = mbi_set_vbe_info(&mb_info, &vbe.controller, vbe.modes_list,
                                &vbe.mode, vbe.current_mode);
      if (status != ERR_SUCCESS) {
         return status;
      }
   }

   if (runtime_addr(boot.name, &addr) == ERR_SUCCESS) {
      mbi_set_boot_loader_name(&mb_info, (uint32_t)addr);
   }

   if (boot.efi_info.valid) {
      status = runtime_addr(boot.efi_info.mmap, &addr);
      if (status != ERR_SUCCESS) {
         return status;
      }

      mbi_set_efi_info(&mb_info, boot.efi_info.systab, addr,
                       boot.efi_info.num_descs,
                       boot.efi_info.desc_size,
                       boot.efi_info.version,
                       boot.efi_info.secure_boot);
   }

   return runtime_addr(&mb_info, run_mbi);
}

/*-- vbe_register --------------------------------------------------------------
 *
 *      Register VBE structures for relocation.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int vbe_register(void)
{
   size_t size;
   int status;

   status = add_sysinfo_object(&vbe.controller, sizeof (vbe_t), ALIGN_PTR);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = add_sysinfo_object(&vbe.mode, sizeof (vbe_mode_t), ALIGN_PTR);
   if (status != ERR_SUCCESS) {
      return status;
   }

   for (size = 0; vbe.modes_list[size] != VBE_MODE_INVAL; size++) {
      ;
   }
   size = (size + 1) * sizeof (vbe_mode_id_t);

   status = add_sysinfo_object(vbe.modes_list, size, ALIGN_PTR);
   if (status != ERR_SUCCESS) {
      return status;
   }

   return ERR_SUCCESS;
}

/*-- multiboot_register -------------------------------------------------------
 *
 *      Register the objects that will need to be relocated.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int multiboot_register(void)
{
   size_t count;
   module_t *mod;
   unsigned int i;
   int status;

   Log(LOG_DEBUG, "Registering Multiboot info...\n");

   status = elf_register(boot.modules[0].addr, &boot.kernel.entry);
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "Kernel registration error.\n");
      return status;
   }

   for (i = 0; i < boot.modules_nr; i++) {
      status = add_sysinfo_object(cmdlines[i], STRSIZE(cmdlines[i]), ALIGN_STR);
      if (status != ERR_SUCCESS) {
         Log(LOG_ERR, "Modules command lines registration error.\n");
         return status;
      }
   }

   if (boot.modules_nr > 0) {
      count = boot.modules_nr - 1;
      status = add_sysinfo_object(mb_mods, count * sizeof (MultiBoot_Module),
                                  ALIGN_PTR);
      if (status != ERR_SUCCESS) {
         return status;
      }

      for (i = 1; i < boot.modules_nr; i++) {
         mod = &boot.modules[i];
         status = add_module_object(mod->addr, mod->size);
         if (status != ERR_SUCCESS) {
            Log(LOG_ERR, "Module registration error.\n");
            return status;
         }
      }
   }

   status = add_sysinfo_object(&mb_info, sizeof (MultiBoot_Info), ALIGN_PTR);
   if (status != ERR_SUCCESS) {
      return status;
   }
   /*
    * Must add the memory map now, but can't convert it from e820 to Multiboot
    * format yet.  Use a conservative maximum size.
    */
   status = add_sysinfo_object(boot.mmap,
                               boot.mmap_count * mb_mmap_desc_size(), 8);
   if (status != ERR_SUCCESS) {
      return status;
   }
   status = add_sysinfo_object(boot.name, STRSIZE(boot.name), ALIGN_STR);
   if (status != ERR_SUCCESS) {
      return status;
   }
   if (boot.efi_info.valid) {
      status = blacklist_runtime_mem(boot.efi_info.systab,
                                     boot.efi_info.systab_size);
      if (status != ERR_SUCCESS) {
         return status;
      }
      status = add_sysinfo_object(boot.efi_info.mmap,
                                  boot.efi_info.desc_size * boot.efi_info.num_descs,
                                  ALIGN_PAGE);
      if (status != ERR_SUCCESS) {
         return status;
      }
   }

   if (vbe.modes_list != NULL) {
      if (vbe_register() != ERR_SUCCESS) {
         Log(LOG_WARNING, "Failed to register VBE structures.\n");
         vbe.modes_list = NULL;
      }
   }

   return ERR_SUCCESS;
}

/*-- multiboot_init_vbe --------------------------------------------------------
 *
 *      Set the kernel preferred video mode, and query the VBE information.
 *      By default, mboot discards the Multiboot video info, and toggles to VGA
 *      text mode before jumping to the kernel. Mboot only provides the VBE
 *      information when the kernel Multiboot headers specify a VBE mode to be
 *      setup.
 *
 * Parameters
 *      IN kbuf:  pointer to the kernel buffer
 *      IN ksize: kernel buffer size, in bytes
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int multiboot_init_vbe(void *kbuf, size_t ksize)
{
   MultiBoot_Header *mbh;
   int status;
   bool text_mode;

   Log(LOG_DEBUG, "Setting up preferred video mode...");

   memset(&vbe, 0, sizeof (vbe_info_t));

   status = video_check_support();
   if (status != ERR_SUCCESS) {
      Log(LOG_WARNING, "Error checking video support: %s", error_str[status]);
      return status;
   }

   mbh = mbh_scan(kbuf, ksize);

   text_mode = true;
   if (mbh != NULL &&
       ((mbh->flags & MBH_FLAG_VIDEO) == MBH_FLAG_VIDEO) &&
       (mbh->mode_type == MBH_VIDEO_GRAPHIC)) {
      status = gui_resize(mbh->width, mbh->height, mbh->depth,
                          mbh->width, mbh->height, mbh->depth);
      if (status == ERR_SUCCESS) {
         text_mode = false;
      } else {
         Log(LOG_WARNING, "Error setting preferred video mode %ux%ux%u: %s",
             mbh->width, mbh->height, mbh->depth, error_str[status]);
      }
   }

   if (text_mode) {
      Log(LOG_DEBUG, "Forcing text mode...");

      status = gui_text();
      if (status != ERR_SUCCESS) {
         Log(LOG_WARNING, "Error setting text mode: %s", error_str[status]);
      }
   }

   if (mbh != NULL && ((mbh->flags & MBH_FLAG_VIDEO) == MBH_FLAG_VIDEO)) {
      int get_info_status = video_get_vbe_info(&vbe);
      if (get_info_status != ERR_SUCCESS) {
         Log(LOG_WARNING, "Error getting video info: %s", error_str[status]);
         if (status == ERR_SUCCESS) {
            status = get_info_status;
         }
      }
   }

   return status;
}

/*-- multiboot_init ------------------------------------------------------------
 *
 *      Allocate the Multiboot Info structure.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int multiboot_init(void)
{
   unsigned int i;
   char *options;
   size_t size;
   int retval;

   cmdlines = sys_malloc(boot.modules_nr * sizeof (char *));
   if (cmdlines == NULL) {
      return ERR_OUT_OF_RESOURCES;
   }

   for (i = 0; i < boot.modules_nr; i++) {
      options = boot.modules[i].options;
      if (options == NULL || options[0] == '\0') {
         retval = asprintf(&cmdlines[i], "%s", boot.modules[i].filename);
      } else {
         retval = asprintf(&cmdlines[i], "%s %s", boot.modules[i].filename,
                           options);
      }

      if (retval == -1) {
         sys_free(cmdlines);
         return ERR_OUT_OF_RESOURCES;
      }
   }

   size = (boot.modules_nr - 1) * sizeof (MultiBoot_Module);

   if (size > 0) {
      mb_mods = sys_malloc(size);
      if (mb_mods == NULL) {
         Log(LOG_ERR, "Not enough memory for the Multiboot module info.\n");
         return ERR_OUT_OF_RESOURCES;
      }
      memset(mb_mods, 0, size);
   }

   memset(&mb_info, 0, sizeof (MultiBoot_Info));

   if (!boot.headless) {
      // Ignore errors; they have been logged already
      (void) multiboot_init_vbe(boot.modules[0].addr, boot.modules[0].size);
   }

   return ERR_SUCCESS;
}

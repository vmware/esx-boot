/*******************************************************************************
 * Copyright (c) 2015-2016 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * mutiboot.c -- Mutiboot support
 */

#include <string.h>
#include <stdio.h>
#include <mutiboot.h>
#include <stdbool.h>
#include <e820.h>
#include <boot_services.h>
#include <cpu.h>

#include "mboot.h"

/* Flags 0-15 are required and must be supported */
#define MUTIBOOT_FLAGS_REQ_MASK    0x0000FFFF

#define MUTIBOOT_GET_REQ_FLAGS(flags) \
   ( (flags) & MUTIBOOT_FLAGS_REQ_MASK )

/* We support only these Mutiboot Header flags */
#define MUTIBOOT_FLAGS_SUPPORTED    (MUTIBOOT_FLAG_VIDEO | \
                                     mutiboot_arch_supported_req_flags())

/*
 * This may become unnecessary at some point, once we're sure
 * we've fully understood and categorized changes in the UEFI
 * memory map between mutiboot_init and e820_to_mutiboot.
 */
#define NUM_E820_SLACK  20

static Mutiboot_Info *mb_info;      /* The Mutiboot Info structure */
static size_t size_mbi;
static Mutiboot_Elmt *next_elmt;    /* Next Mutiboot element to use */
static char **cmdlines;
static vbe_info_t vbe;              /* VBE information */

/*-- mutiboot_scan -------------------------------------------------------------
 *
 *      Locates the Mutiboot Header within a given buffer.
 *
 *      Mutiboot uses a header similar to Multiboot. The header is located
 *      within the first MUTIBOOT_SEARCH bytes of the first (as in
 *      lowest-loaded) ELF segment. The Mutiboot header must be 64-bit aligned.
 *
 * Parameters
 *      IN buffer: pointer to the buffer to search in
 *      IN buflen: buffer size, in bytes
 *
 * Results
 *      A pointer to the Mutiboot header, or NULL if the header was not found.
 *----------------------------------------------------------------------------*/
static INLINE Mutiboot_Header *mutiboot_scan(void *buffer, size_t buflen)
{
   Mutiboot_Header *mbh = buffer;

   for (buflen = MIN(buflen, MUTIBOOT_SEARCH); buflen >= sizeof (Mutiboot_Header);
        buflen -= MUTIBOOT_ALIGNMENT) {

      if ((mbh->magic == MUTIBOOT_MAGIC) &&
          ((mbh->magic + mbh->flags + mbh->checksum) == 0)) {
         return mbh;
      }

      mbh = (Mutiboot_Header *)((char *)mbh + MUTIBOOT_ALIGNMENT);
   }

   return NULL;
}

static void mb_mmap_sanity_check(void)
{
   uint64_t max_base, max_limit;
   Mutiboot_MemRange *mbi_mem;
   bool error = false;
   bool overlap = false;
   size_t i;

   max_base = 0;
   max_limit = 0;
   i = 0;
   FOR_EACH_MUTIBOOT_ELMT_TYPE_DO(mb_info, MUTIBOOT_MEMRANGE_TYPE, mbi_mem) {
      uint64_t base, len, limit;
      const char *msg = NULL;

      base = mbi_mem->startAddr;
      len = mbi_mem->len;
      limit = base + len - 1;

      if (base < max_base) {
         error = true;
         msg = "Mutiboot MemMap is not sorted";
         Log(LOG_ERR, "mmap[%zu]: %"PRIx64" - %"PRIx64" type %u: %s.\n",
             i, base, limit, mbi_mem->memType, msg);
      }

      if (len > 0 && limit < max_limit) {
         overlap = true;
      }

      max_base = base;
      max_limit = limit;
      i++;
   } FOR_EACH_MUTIBOOT_ELMT_TYPE_DONE(mb_info, mbi_mem);

   if (overlap || error) {
      FOR_EACH_MUTIBOOT_ELMT_TYPE_DO(mb_info, MUTIBOOT_MEMRANGE_TYPE, mbi_mem) {
         uint64_t base, len, limit;

         base = mbi_mem->startAddr;
         len = mbi_mem->len;
         limit = base + len - 1;

         Log(LOG_DEBUG, "mmap[%zu]: %"PRIx64" - %"PRIx64" type %u\n",
             i, base, limit, mbi_mem->memType);
      } FOR_EACH_MUTIBOOT_ELMT_TYPE_DONE(mb_info, mbi_mem);

      if (overlap) {
         Log(LOG_WARNING, "Mutiboot MemMap contains overlapping ranges.\n");
      }

      if (error) {
         Log(LOG_ERR, "Mutiboot MemMap is corrupted.\n");
         PANIC();
      }
   }
}

/*-- mb_advance_next_elmt-------------------------------------------------------
 *
 *      Advance next_elmt to next free space in mb_info buffer.  Does
 *      not check for overflow; use mb_check_space as soon as the
 *      required size for the next element is known.
 *----------------------------------------------------------------------------*/
static void mb_advance_next_elmt(void)
{
   mb_info->numMutibootElmt++;
   next_elmt = (Mutiboot_Elmt *)((uint8_t *)next_elmt + next_elmt->elmtSize);
}

/*-- mb_check_space ------------------------------------------------------------
 *
 *      Check for available space in mb_info at next_elmt.
 *
 * Parameters
 *      IN size:       amount of space needed
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int mb_check_space(size_t size)
{
   size_t used = (uint8_t *)next_elmt - (uint8_t *)mb_info;

   if (size <= size_mbi - used) {
      return ERR_SUCCESS;
   }
   Log(LOG_ERR, "Mutiboot buffer is too small (wanted %zu, have %zu/%zu).\n",
       size, size_mbi - used, size_mbi);
   return ERR_BUFFER_TOO_SMALL;
}

/*-- mb_set_mmap_entry ---------------------------------------------------------
 *
 *      Setup a Mutiboot memory map entry.
 *
 * Parameters
 *      IN base:       memory region starting address
 *      IN len:        memory region size, in bytes
 *      IN type:       memory region e820 type
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int mb_set_mmap_entry(uint64_t base, uint64_t len, uint32_t type)
{
   int status;
   Mutiboot_MemRange *memRange = (Mutiboot_MemRange *)next_elmt;

   status = mb_check_space(sizeof(Mutiboot_MemRange));
   if (status != ERR_SUCCESS) {
      return status;
   }

   memRange->type = MUTIBOOT_MEMRANGE_TYPE;
   memRange->elmtSize = sizeof(Mutiboot_MemRange);

   memRange->startAddr = base;
   memRange->len = len;
   memRange->memType = type;

   mb_advance_next_elmt();
   return ERR_SUCCESS;
}

/*-- check_mutiboot_kernel ----------------------------------------------------
 *
 *      Check whether the given buffer contains a valid Mutiboot kernel.
 *
 * Parameters
 *      IN kbuf:  pointer to the kernel buffer
 *      IN ksize: kernel buffer size, in bytes
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int check_mutiboot_kernel(void *kbuf, size_t ksize)
{
   Mutiboot_Header *mbh;
   int status;
   size_t i;
   Elf_CommonAddr base;

   if (kbuf == NULL || ksize == 0) {
      return ERR_INVALID_PARAMETER;
   }

   if (ksize < sizeof (Mutiboot_Header)) {
      Log(LOG_ERR, "Kernel is too small.\n");
      return ERR_BAD_TYPE;
   }

   status = elf_check_headers(kbuf, ksize, &base);
   if (IS_WARNING(status)) {
      Log(LOG_WARNING, "Funny-looking ELF\n");
   } else if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "Invalid ELF binary.\n");
      return status;
   }

   /*
    * We'll actually hunt for the mutiboot header
    * in the first program header. On ARM64 there's
    * crazy 64-kb alignment that removes any chances
    * of actually locating the header in the first 8kb.
    */
   for (i = 0; i < MUTIBOOT_ALIGNMENT; i++) {
      mbh = mutiboot_scan((char *) (uintptr_t) base + i,
                          ksize - i - (base - (uintptr_t) kbuf));
      if (mbh != NULL) {
         break;
      }
   }

   if (mbh == NULL) {
      Log(LOG_DEBUG, "Mutiboot header is not found.\n");
      return ERR_BAD_TYPE;
   }

   if (i > 0) {
      Log(LOG_ERR, "Mutiboot header is not %u-bytes aligned.\n",
          MUTIBOOT_ALIGNMENT);
      return ERR_BAD_TYPE;
   }

   if ((MUTIBOOT_GET_REQ_FLAGS(mbh->flags) & ~MUTIBOOT_FLAGS_SUPPORTED) != 0) {
      Log(LOG_ERR, "Mutiboot header contains unsupported flags.\n");
      Log(LOG_ERR, "req. flags set: 0x%x (supported 0x%x) \n",
          MUTIBOOT_GET_REQ_FLAGS(mbh->flags),
          MUTIBOOT_FLAGS_SUPPORTED);
      return ERR_BAD_TYPE;
   }

   if (!mutiboot_arch_check_kernel(mbh)) {
      /*
       * mutiboot_arch_check_kernel will log as appropriate.
       */
      return ERR_BAD_TYPE;
   }

   boot.efi_info.rts_size = 0;
   boot.efi_info.rts_vaddr = 0;
   boot.efi_info.caps |= EFI_RTS_CAP_RTS_SIMPLE;
   if ((mbh->flags & MUTIBOOT_FLAG_EFI_RTS_OLD) != 0) {
      boot.efi_info.rts_vaddr = mbh->rts_vaddr;
      /*
       * The old way stuffed RTS into DirectMap,
       * and this is the implicit size of that region.
       */
      boot.efi_info.rts_size = (64UL*1024*1024*1024*1024);
   }

   if ((mbh->flags & MUTIBOOT_FLAG_EFI_RTS_NEW) != 0) {
      boot.efi_info.rts_vaddr = mbh->rts_vaddr;
      boot.efi_info.rts_size = mbh->rts_size;
      boot.efi_info.caps |= EFI_RTS_CAP_RTS_SPARSE |
         EFI_RTS_CAP_RTS_COMPACT |
         EFI_RTS_CAP_RTS_CONTIG;
   }

   return ERR_SUCCESS;
}

/*-- mbi_set_modules_info ------------------------------------------------------
 *
 *      Set modules-related fields in the MBI. This includes the modules command
 *      lines and locations, as well as the modules table itself.
 *
 * Parameters
 *      IN mods:       pointer to the module info structure
 *      IN mods_count: number of modules
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int mbi_set_modules_info(module_t *mods, unsigned int mods_count)
{
   run_addr_t addr, cmdline;
   unsigned int i;
   int status;

   for (i = 0; i < mods_count; i++) {
      Mutiboot_Module *mod = (Mutiboot_Module *)next_elmt;

      status = mb_check_space(sizeof(Mutiboot_Module) +
                              sizeof(Mutiboot_ModuleRange));
      if (status != ERR_SUCCESS) {
         return status;
      }

      status = runtime_addr(cmdlines[i + 1], &cmdline);
      if (status != ERR_SUCCESS) {
         return status;
      }

      mod->type = MUTIBOOT_MODULE_TYPE;
      mod->elmtSize = sizeof(Mutiboot_Module);

      mod->string = cmdline;
      mod->moduleSize = mods[i].size;

      if (mods[i].size > 0) {
         status = runtime_addr(mods[i].addr, &addr);
         if (status != ERR_SUCCESS) {
            return status;
         }
         mod->numRanges = 1;
         mod->ranges[0].startPageNum = addr / PAGE_SIZE;
         mod->ranges[0].numPages = PAGE_ALIGN_UP(mods[i].size) / PAGE_SIZE;

         mod->elmtSize += sizeof(Mutiboot_ModuleRange);
      } else {
         addr = 0;
         mod->numRanges = 0;
      }

      mb_advance_next_elmt();
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
 *      IN mbi:    pointer to the Mutiboot Info structure
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int mbi_set_kernel_info(Mutiboot_Info *mbi)
{
   run_addr_t addr;
   int status;

   status = runtime_addr(cmdlines[0], &addr);
   if (status != ERR_SUCCESS) {
      return status;
   }

   if (!(strlen(cmdlines[0]) < MUTIBOOT_MAXCMDLINE)) {
      Log(LOG_CRIT, "Boot command line exceeds maximum supported length.");
      return ERR_UNSUPPORTED;
   }

   mbi->cmdline = addr;

   return status;
}

/*-- mbi_set_vbe_info ----------------------------------------------------------
 *
 *      Set VBE-related fields in the MBI.
 *
 * Parameters
 *      IN vbe_info:  pointer to the VBE controller info structure
 *      IN vbe_modes: pointer to the supported VBE modes list
 *      IN mode_info: pointer to the current VBE mode info structure
 *      IN mode_id:   current VBE mode identifier
 *      IN fb_addr:   current VBE mode framebuffer address
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int mbi_set_vbe_info(vbe_t *vbe_info,
                            vbe_mode_id_t *vbe_modes, vbe_mode_t *mode_info,
                            vbe_mode_id_t mode_id, uintptr_t fb_addr)
{
   run_addr_t info, mode, modes_list;
   int status;
   Mutiboot_Vbe *mb_vbe = (Mutiboot_Vbe *)next_elmt;

   status = mb_check_space(sizeof(Mutiboot_Vbe));
   if (status != ERR_SUCCESS) {
      return status;
   }

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

   mb_vbe->type = MUTIBOOT_VBE_TYPE;
   mb_vbe->elmtSize = sizeof(Mutiboot_Vbe);

   mb_vbe->vbe_control_info = info;
   mb_vbe->vbe_mode_info = mode;
   mb_vbe->vbe_mode = mode_id;
   mb_vbe->vbe_flags = MUTIBOOT_VBE_FB64;
   mb_vbe->fbBaseAddress = fb_addr;

   mb_advance_next_elmt();
   return ERR_SUCCESS;
}

/*-- set_efi_info -------------------------------------------------------------
 *
 *      Set EFI-related fields in the MBI.
 *
 * Parameters
 *      IN systab:         Address of the EFI systab.
 *      IN mmap:           Address of the UEFI memory descriptor map.
 *      IN mmap_num_descs: Number of UEFI memory map descriptors.
 *      IN mmap_desc_size: Size of each UEFI memory map descriptor.
 *      IN mmap_version:   Version of each UEFI memory map descriptor.
 *      IN secure_boot:    TRUE if UEFI secure boot is in progress.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int set_efi_info(uint64_t systab,
                        uint64_t mmap,
                        uint32_t mmap_num_descs,
                        uint32_t mmap_desc_size,
                        uint32_t mmap_version,
                        bool secure_boot)
{
   int status;
   Mutiboot_Efi *mbefi = (Mutiboot_Efi *)next_elmt;

   status = mb_check_space(sizeof(Mutiboot_Efi));
   if (status != ERR_SUCCESS) {
      return status;
   }

   mbefi->type = MUTIBOOT_EFI_TYPE;
   mbefi->elmtSize = sizeof(Mutiboot_Efi);

   mbefi->efi_flags = MUTIBOOT_EFI_MMAP;
   if (arch_is_64) {
      mbefi->efi_flags |= MUTIBOOT_EFI_ARCH64;
   }
   if (secure_boot) {
      mbefi->efi_flags |= MUTIBOOT_EFI_SECURE_BOOT;
   }

   mbefi->efi_systab = systab;
   mbefi->efi_mmap = mmap;
   mbefi->efi_mmap_num_descs = mmap_num_descs;
   mbefi->efi_mmap_desc_size = mmap_desc_size;
   mbefi->efi_mmap_version = mmap_version;

   mb_advance_next_elmt();
   return ERR_SUCCESS;
}

/*-- e820_to_mutiboot ---------------------------------------------------------
 *
 *      Convert a E820 memory map to the Mutiboot memory map format.
 *
 * Parameters
 *      IN e820:      E820 memory map
 *      IN/OUT count: number of entries in the E820 memory map
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *
 * Side effects
 *      The E820 map is modified in place, with E820_TYPE_BOOTLOADER entries
 *      changed to the E820_TYPE_AVAILABLE type and coalesced (merged).
 *      The modified E820 map is used to generate the converted memory map.
 *----------------------------------------------------------------------------*/
static int e820_to_mutiboot(e820_range_t *e820, size_t *count)
{
   e820_range_t *range = e820;
   e820_range_t *last = range + *count;

   if (*count == 0) {
      return ERR_INVALID_PARAMETER;
   }

   while (range < last) {
      if (range->type == E820_TYPE_BOOTLOADER) {
         range->type = E820_TYPE_AVAILABLE;
      }
      range++;
   }

   Log(LOG_DEBUG, "E820 count before final merging: %zu\n", *count);
   e820_mmap_merge(e820, count);
   Log(LOG_DEBUG, "E820 count after final merging: %zu\n", *count);

   while ((*count)-- > 0) {
      uint64_t base, length;
      int status;

      base = E820_BASE(e820);
      length = E820_LENGTH(e820);

      status = mb_set_mmap_entry(base, length, e820->type);
      if (status != ERR_SUCCESS) {
         return status;
      }
      e820++;
   }

   return ERR_SUCCESS;
}

/*-- mutiboot_set_runtime_pointers --------------------------------------------
 *
 *      1) Convert boot.mmap from e820 format to mutiboot format.
 *
 *      2) Setup the Mutiboot Info structure internal pointers to their
 *      run-time (relocated) values.
 *
 * Parameters
 *      OUT run_mbi: address of the relocated Mutiboot Info structure.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int mutiboot_set_runtime_pointers(run_addr_t *run_mbi)
{
   int status;

   Log(LOG_DEBUG, "Converting e820 map to Mutiboot format...\n");

   status = e820_to_mutiboot(boot.mmap, &boot.mmap_count);
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "Mutiboot memory map error.\n");
      return status;
   }

   mb_mmap_sanity_check();

   Log(LOG_DEBUG, "Setting up Mutiboot runtime references...\n");

   status = mbi_set_modules_info(&boot.modules[1], boot.modules_nr - 1);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = mbi_set_kernel_info(mb_info);
   if (status != ERR_SUCCESS) {
      return status;
   }

   if (vbe.modes_list != NULL) {
      status = mbi_set_vbe_info(&vbe.controller, vbe.modes_list,
                                &vbe.mode, vbe.current_mode, vbe.fb_addr);
      if (status != ERR_SUCCESS) {
         return status;
      }
   }

   if (boot.efi_info.valid) {
      run_addr_t addr;

      status = runtime_addr(boot.efi_info.mmap, &addr);
      if (status != ERR_SUCCESS) {
         return status;
      }

      status = set_efi_info(boot.efi_info.systab, addr,
                            boot.efi_info.num_descs,
                            boot.efi_info.desc_size,
                            boot.efi_info.version,
                            boot.efi_info.secure_boot);
      if (status != ERR_SUCCESS) {
         return status;
      }
   }

   return runtime_addr(mb_info, run_mbi);
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

/*-- mutiboot_register -------------------------------------------------------
 *
 *      Register the objects that will need to be relocated.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int mutiboot_register(void)
{
   module_t *mod;
   unsigned int i;
   int status;

   Log(LOG_DEBUG, "Registering Mutiboot info...\n");

   status = elf_register(boot.modules[0].addr, &boot.kernel.entry);
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "Kernel registration error.\n");
      return status;
   }

   /*
    * Ensure the MBI and all subsequent system objects start on a page boundary.
    */
   status = add_sysinfo_object(mb_info, size_mbi, ALIGN_PAGE);
   if (status != ERR_SUCCESS) {
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
      for (i = 1; i < boot.modules_nr; i++) {
         mod = &boot.modules[i];
         status = add_module_object(mod->addr, mod->size);
         if (status != ERR_SUCCESS) {
            Log(LOG_ERR, "Module registration error.\n");
            return status;
         }
      }
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

/*-- mutiboot_init_vbe --------------------------------------------------------
 *
 *      Set the kernel preferred video mode, and query the VBE information.
 *      By default, mboot discards the Mutiboot video info, and toggles to VGA
 *      text mode before jumping to the kernel. Mboot only provides the VBE
 *      information when the kernel Mutiboot headers specify a VBE mode to be
 *      setup.
 *
 * Parameters
 *      IN kbuf:  pointer to the kernel buffer
 *      IN ksize: kernel buffer size, in bytes
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int mutiboot_init_vbe(void *kbuf, size_t ksize)
{
   Mutiboot_Header *mbh;
   int status;
   bool text_mode;

   Log(LOG_DEBUG, "Setting up preferred video mode...");

   memset(&vbe, 0, sizeof (vbe_info_t));

   status = video_check_support();
   if (status != ERR_SUCCESS) {
      Log(LOG_WARNING, "Error checking video support: %s", error_str[status]);
      return status;
   }

   mbh = mutiboot_scan(kbuf, ksize);

   text_mode = true;
   if (mbh != NULL &&
       ((mbh->flags & MUTIBOOT_FLAG_VIDEO) == MUTIBOOT_FLAG_VIDEO) &&
       (mbh->mode_type == MUTIBOOT_VIDEO_GRAPHIC)) {
      unsigned int min_width, min_height, min_depth;
      if ((mbh->flags & MUTIBOOT_FLAG_VIDEO_MIN) == MUTIBOOT_FLAG_VIDEO_MIN) {
         min_width = mbh->min_width;
         min_height = mbh->min_height;
         min_depth = mbh->min_depth;
      } else {
         min_width = mbh->width;
         min_height = mbh->height;
         min_depth = mbh->depth;
      }
      status = gui_resize(mbh->width, mbh->height, mbh->depth,
                          min_width, min_height, min_depth);
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

   if (mbh != NULL &&
       ((mbh->flags & MUTIBOOT_FLAG_VIDEO) == MUTIBOOT_FLAG_VIDEO)) {
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

/*-- mutiboot_init ------------------------------------------------------------
 *
 *      Allocate the Mutiboot Info structure.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int mutiboot_init(void)
{
   size_t size_mod;
   unsigned int i;
   int status;
   e820_range_t *e820;
   size_t num_e820_ranges;

   /*
    * Let's estimate the numbers of memory ranges we would have to
    * store.
    */
   status = get_memory_map(0, &e820, &num_e820_ranges, &boot.efi_info);
   if (status != ERR_SUCCESS) {
      return status;
   }
   Log(LOG_DEBUG, "E820 count estimate: %zu+%u slack\n",
       num_e820_ranges, NUM_E820_SLACK);
   free_memory_map(e820, &boot.efi_info);

   size_mod = sizeof(Mutiboot_Module) + sizeof(Mutiboot_ModuleRange);

   size_mbi  = sizeof(Mutiboot_Info);
   size_mbi += sizeof(Mutiboot_MemRange) * (num_e820_ranges + NUM_E820_SLACK);
   size_mbi += size_mod * boot.modules_nr;
   size_mbi += sizeof(Mutiboot_Vbe);

#ifndef __COM32__
   /*
    * UEFI is being used.
    */
   size_mbi += sizeof(Mutiboot_Efi);
#endif

   mb_info = sys_malloc(size_mbi);
   if (mb_info == NULL) {
      return ERR_OUT_OF_RESOURCES;
   }

   cmdlines = sys_malloc(boot.modules_nr * sizeof (char *));
   if (cmdlines == NULL) {
      sys_free(mb_info);
      return ERR_OUT_OF_RESOURCES;
   }

   mb_info->numMutibootElmt = 0;
   next_elmt = &mb_info->elmts[0];

   for (i = 0; i < boot.modules_nr; i++) {
      int retval;
      char *options = boot.modules[i].options;

      if (options == NULL || options[0] == '\0') {
         retval = asprintf(&cmdlines[i], "%s", boot.modules[i].filename);
      } else {
         retval = asprintf(&cmdlines[i], "%s %s", boot.modules[i].filename,
                           options);
      }

      // cmdlines[0] includes boot cmd line
      if ( (retval >= MUTIBOOT_MAXMODNAME) && (i != 0) ){
         Log(LOG_CRIT, "Boot module string exceeds maximum supported length.");
         return ERR_UNSUPPORTED;
      }

      if (retval == -1) {
         sys_free(cmdlines);
         sys_free(mb_info);
         return ERR_OUT_OF_RESOURCES;
      }
   }

   if (!boot.headless) {
      // Ignore errors; they have been logged already
      (void) mutiboot_init_vbe(boot.modules[0].addr, boot.modules[0].size);
   }

   return ERR_SUCCESS;
}

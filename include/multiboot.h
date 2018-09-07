/*******************************************************************************
 * Copyright (c) 2008-2011,2015-2016 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * multiboot.h -- Multiboot specification definitions
 *
 * Multiboot has been deprecated in favor of Mutiboot, and is only
 * supported by esx-boot to boot older version of vSphere.
 *
 * Most of this file reflects the "official" multiboot specification, published
 * by the GNU project, available at:
 * http://www.gnu.org/software/grub/manual/multiboot/multiboot.html
 *
 * Several extensions to the aformentioned specification have been made, to
 * support booting on EFI platforms. Those extensions are detailed below.
 *
 * 1. Identification of EFI firmware
 *
 *      An OS can determine if it was booted from EFI firmware or from a legacy
 *      BIOS by checking the MULTIBOOT_FLAG_EFI_VALID flag in the multiboot
 *      information structure's flags. If the machine uses EFI firmware, then
 *      this flag must be set, and the efi_* fields of the multiboot
 *      information structure must be set to valid values.
 *
 *      If the machine uses 32-bit x86 EFI firmware, then the efi_arch field
 *      must be 0. If the machine uses 64-bit x86_64 EFI firmware, then the
 *      efi_arch field must be 1.
 *
 * 2. Location of the EFI system table
 *
 *      On an EFI system, the physical address of the EFI system table must be
 *      passed to the OS in the efi_systab_low and efi_systab_high fields.
 *
 * 3. EFI memory map
 *
 *      The physical address of a buffer containing the EFI memory map must be
 *      passed in efi_mmap. It contains a number of EFI_MEMORY_DESCRIPTOR
 *      structures as defined in the EFI specification. The number of
 *      descriptors is passed in efi_mmap_num_descs, the size of each
 *      descriptor is passed in efi_mmap_desc_size, and the descriptor version
 *      is passed in efi_mmap_version. For more details on what these values
 *      mean, see the documentation for GetMemoryMap in the EFI specification.
 *
 *      Even if an EFI memory map is provided, the bootloader should still
 *      provide an E820-like memory map, and set the mmap_length and mmap_addr
 *      fields to appropriate values. Descriptors in the E820 map that refer to
 *      EFI runtime services memory must be marked as reserved. (See section
 *      14.3 in the ACPI specification, version 4.0a, for the complete mapping
 *      of memory types.)
 */

#ifndef MULTIBOOT_H_
#define MULTIBOOT_H_

#include <sys/types.h>
#include <bootlib.h>

/*-- MultiBoot Header ----------------------------------------------------------
 *
 * The Multiboot Header must be 4-bytes aligned, and must fit entirely within
 * the first 8192 bytes of the kernel image.
 */
#define MBH_MAGIC             0x1BADB002 /* Multiboot header signature */
#define MBH_ALIGNMENT         4
#define MBH_SEARCH            8192

#define MBH_FLAG_PAGE_ALIGN   (1 << 0)   /* Align modules on page (4KB) */
#define MBH_FLAG_MEMORY       (1 << 1)   /* Must pass memory info to OS */
#define MBH_FLAG_VIDEO        (1 << 2)   /* Must pass video info to OS */
#define MBH_FLAG_AOUT_KLUDGE  (1 << 16)  /* Address fields are valid */
#define MBH_FLAG_EFI_RTS_OLD  (1 << 17)  /* rts_vaddr field is valid */
#define MBH_FLAG_EFI_RTS_NEW  (1 << 18)  /* rts_vaddr/rts_size fields are valid */

#define MBH_VIDEO_GRAPHIC     0          /* Linear graphics mode */
#define MBH_VIDEO_TEXT        1          /* EGA-standard text mode */

#pragma pack(1)
typedef struct MultiBoot_Header {
   uint32_t magic;         /* Multiboot Header Magic */
   uint32_t flags;         /* Feature flags */
   uint32_t checksum;      /* (magic + flags + checksum) sum up to zero */
   uint32_t header_addr;
   uint32_t load_addr;
   uint32_t load_end_addr;
   uint32_t bss_end_addr;
   uint32_t entry_addr;
   uint32_t mode_type;     /* Video mode (0=graphic, 1=text) */
   uint32_t width;         /* Video mode horizontal resolution */
   uint32_t height;        /* Video mode vertical resolution */
   uint32_t depth;         /* 0 in text mode, bits per pixel in graphic mode */
   uint64_t rts_vaddr;     /* Virtual address of UEFI Runtime Services */
   uint64_t rts_size;      /* For new-style RTS. */
} MultiBoot_Header;
#pragma pack()

/*-- mbh_scan ------------------------------------------------------------------
 *
 *      Locates the Multiboot Header within a given buffer.
 *
 *      Multiboot Specification 0.6.96 says:
 *       "An OS image must contain an additional header called Multiboot header,
 *        besides the headers of the format used by the OS image. The Multiboot
 *        header must be contained completely within the first 8192 bytes of the
 *        OS image, and must be longword (32-bit) aligned. In general, it should
 *        come as early as possible, and may be embedded in the beginning of the
 *        text segment after the real executable header."
 *
 *      NOTE: It is not impossible to encounter a Multiboot Header Magic in a
 *            header preceding the actual Multiboot header (for instance, in the
 *            ELF headers). For reducing the chances of confusion, we consider
 *            that the Multiboot Header must hold a valid checksum.
 *
 * Parameters
 *      IN buffer: pointer to the buffer to search in
 *      IN buflen: buffer size, in bytes
 *
 * Results
 *      A pointer to the Multiboot header, or NULL if the header was not found.
 *----------------------------------------------------------------------------*/
static INLINE MultiBoot_Header *mbh_scan(void *buffer, size_t buflen)
{
   MultiBoot_Header *mbh = buffer;

   for (buflen = MIN(buflen, 8192); buflen >= sizeof (MultiBoot_Header);
        buflen -= MBH_ALIGNMENT) {

      if ((mbh->magic == MBH_MAGIC) &&
          ((mbh->magic + mbh->flags + mbh->checksum) == 0)) {
         return mbh;
      }

      mbh = (MultiBoot_Header *)((char *)mbh + MBH_ALIGNMENT);
   }

   return NULL;
}

/*-- MultiBoot Info ------------------------------------------------------------
 *
 * Upon entry to the operating system, the EBX register contains the physical
 * address of a Multiboot information data structure, through which the boot
 * loader communicates vital information to the operating system. The operating
 * system can use or ignore any parts of the structure as it chooses; all
 * information passed by the boot loader is advisory only.
 *
 * The Multiboot information structure and its related substructures may be
 * placed anywhere in memory by the boot loader (with the exception of the
 * memory reserved for the kernel and boot modules, of course). It is the
 * operating system's responsibility to avoid overwriting this memory until it
 * is done using it.
 */

#define MBI_MAGIC                   0x2BADB002

#define MBI_FLAG_MEM_VALID          (1 << 0)   /* mem fields are valid */
#define MBI_FLAG_BOOTDEV_VALID      (1 << 1)   /* boot device field is valid */
#define MBI_FLAG_CMDLINE_VALID      (1 << 2)   /* cmdline field is valid */
#define MBI_FLAG_MOD_VALID          (1 << 3)   /* mod fields are valid */
#define MBI_FLAG_AOUT_VALID         (1 << 4)   /* aout field is valid */
#define MBI_FLAG_ELF_VALID          (1 << 5)   /* elf field is valid */
#define MBI_FLAG_MMAP_VALID         (1 << 6)   /* mmap fields are valid */
#define MBI_FLAG_LOADER_NAME_VALID  (1 << 9)   /* boot_loader_name is valid */
#define MBI_FLAG_VIDEO_VALID        (1 << 11)  /* VBE information is valid */
#define MBI_FLAG_EFI_VALID          (1 << 12)  /* EFI information is valid */
#define MBI_FLAG_EFI_MMAP           (1 << 13)  /* EFI memory map information is valid */

#define MBI_EFI_FLAG_ARCH64         (1 << 0)   /* 64-bit EFI */
#define MBI_EFI_FLAG_SECURE_BOOT    (1 << 1)   /* EFI Secure Boot in progress */

#define MBI_LOWER_MEM_END           0xA0000
#define MBI_UPPER_MEM_START         0x100000

/*
 * Size in bytes of a Multiboot memory map descriptor which only contains the
 * standard fields. This size includes the 4 bytes of the 'size' descriptor
 * field.
 */
#define MBI_MMAP_ENTRY_MIN_SIZE      24

#pragma pack(1)
typedef struct MultiBoot_AoutInfo {
   uint32_t tabsize;
   uint32_t strsize;
   uint32_t addr;
   uint32_t reserved;
} MultiBoot_AoutInfo;

typedef struct MultiBoot_ElfInfo {
   uint32_t num;               /* Nb of entries in the section header table */
   uint32_t size;              /* Size of a section header table entry */
   uint32_t addr;              /* ELF ection header table physical address */
   uint32_t shndx;             /* Strings index in the section header table */
} MultiBoot_ElfInfo;

typedef struct MultiBoot_Info {
   uint32_t flags;              /* Multiboot info header feature flags */
   uint32_t mem_lower;          /* Free memory starting at address 0, in KB */
   uint32_t mem_upper;          /* Free memory from 1MB to first hole, in KB */
   uint32_t boot_device;        /* Device kernel was booted from if disk */
   uint32_t cmdline;            /* Kernel command line physical address */
   uint32_t mods_count;         /* Number of boot modules */
   uint32_t mods_addr;          /* Modules table physical address */
   union {
      MultiBoot_AoutInfo aout;
      MultiBoot_ElfInfo elf;
   } u;
   uint32_t mmap_length;        /* Memory map size, in bytes */
   uint32_t mmap_addr;          /* Memory map physical address */
   uint32_t drives_length;
   uint32_t drives_addr;
   uint32_t config_table;
   uint32_t boot_loader_name;
   uint32_t apm_table;
   uint32_t vbe_control_info;
   uint32_t vbe_mode_info;
   uint16_t vbe_mode;
   uint16_t vbe_interface_seg;
   uint16_t vbe_interface_off;
   uint16_t vbe_interface_len;

   /* Fields below are EFI-specific (only valid if flags[12] is set) */
   uint32_t efi_flags;
   uint32_t efi_systab_low;     /* EFI system table physical address */
   uint32_t efi_systab_high;    /* EFI system table physical address */

   /* Fields below are EFI-specific (only valid if flags[12] and flags[13] are set) */
   uint32_t efi_mmap;           /* EFI memory map physical address */
   uint32_t efi_mmap_num_descs; /* Number of descriptors in EFI memory map */
   uint32_t efi_mmap_desc_size; /* Size of each descriptor */
   uint32_t efi_mmap_version;   /* Descriptor version */
} MultiBoot_Info;

typedef struct MultiBoot_Module {
   uint32_t mod_start;  /* physical address of start of module */
   uint32_t mod_end;    /* physical address of end of module */
   uint32_t string;     /* Module command line physical address */
   uint32_t reserved;
} MultiBoot_Module;

typedef struct MultiBoot_MemMap {
   uint32_t size;         /* size of this structure MINUS this field */
   uint32_t lowAddr;      /* low 32 bits of base address */
   uint32_t highAddr;     /* high 32 bits of base address */
   uint32_t lowLen;       /* low 32 bits of length */
   uint32_t highLen;      /* high 32 bits of length */
   uint32_t type;         /* ACPI memory type */
} MultiBoot_MemMap;
#pragma pack()

#define MBI_MMAP_EXTENDED_ATTR(_entry_)               \
   ((void *)((char *)(_entry_) + MBI_MMAP_ENTRY_MIN_SIZE))

static INLINE void mbi_set_module(MultiBoot_Module *mod, uint32_t name,
                                  uint32_t start, uint32_t size)
{
   mod->string = name;
   mod->mod_start = start;
   mod->mod_end = start + size;
}

static INLINE void mbi_set_mods_table(MultiBoot_Info *mbi, uint32_t addr,
                                      uint32_t count)
{
   mbi->mods_count = count;
   mbi->mods_addr = addr;
   mbi->flags |= MBI_FLAG_MOD_VALID;
}

static INLINE void mbi_set_mmap(MultiBoot_Info *mbi, uint32_t mmap,
                                uint32_t size)
{
   mbi->mmap_addr = mmap;
   mbi->mmap_length = size;
   mbi->flags |= MBI_FLAG_MMAP_VALID;
}

static INLINE void mbi_set_mem(MultiBoot_Info *mbi, uint32_t mem_lower,
                               uint32_t mem_upper)
{
   mbi->mem_lower = mem_lower;
   mbi->mem_upper = mem_upper;
   mbi->flags |= MBI_FLAG_MEM_VALID;
}

static INLINE void mbi_set_cmdline(MultiBoot_Info *mbi, uint32_t cmdline)
{
   mbi->cmdline = cmdline;
   mbi->flags |= MBI_FLAG_CMDLINE_VALID;
}

static INLINE void mbi_set_boot_loader_name(MultiBoot_Info *mbi, uint32_t name)
{
   mbi->boot_loader_name = name;
   mbi->flags |= MBI_FLAG_LOADER_NAME_VALID;
}

static INLINE void mbi_set_vbe(MultiBoot_Info *mbi, uint32_t vbe_control_info,
                               uint32_t vbe_mode_info, uint16_t mode)
{
   mbi->vbe_control_info = vbe_control_info;
   mbi->vbe_mode_info = vbe_mode_info;
   mbi->vbe_mode = mode;
   mbi->flags |= MBI_FLAG_VIDEO_VALID;
}

static INLINE void mbi_set_efi_info(MultiBoot_Info *mbi, uint64_t systab,
                                    uint32_t mmap, uint32_t mmap_num_descs,
                                    uint32_t mmap_desc_size,
                                    uint32_t mmap_version,
                                    bool secure_boot)
{
   mbi->efi_flags = 0;
   /*
    * Note: do not add new flags to efi_flags in multiboot.h, as old
    * kernels will panic if unexpected flags are set.  Add new flags
    * only in mutiboot.h.
    */
#if defined(only_em64t)
   mbi->efi_flags |= MBI_EFI_FLAG_ARCH64;
#endif
   if (secure_boot) {
      mbi->efi_flags |= MBI_EFI_FLAG_SECURE_BOOT;
   }
   mbi->efi_systab_low = lowhalf64(systab);
   mbi->efi_systab_high = highhalf64(systab);
   mbi->efi_mmap = mmap;
   mbi->efi_mmap_num_descs = mmap_num_descs;
   mbi->efi_mmap_desc_size = mmap_desc_size;
   mbi->efi_mmap_version = mmap_version;
   mbi->flags |= MBI_FLAG_EFI_VALID | MBI_FLAG_EFI_MMAP;
}

#endif /* !MULTIBOOT_H_ */

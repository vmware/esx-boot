/*******************************************************************************
 * Copyright (c) 2015-2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * mutiboot.h --
 *
 *       The Mutiboot boot loader interface is a redesign of the
 *       Multiboot interface(*).  It takes over some design concepts,
 *       but widens all address fields to 64 bits and omits obsolete
 *       features.
 *
 *       Mutiboot is built around a variable size array of elements,
 *       where each element has its own type and size.  The
 *       specification can easily be extended in the future in a
 *       compatible manner by adding more element types and sizes.
 *
 *       Mutiboot is architecture and platform-agnostic.
 *
 *       (* The Multiboot specification document can be found at
 *       http://www.gnu.org/software/grub/manual/multiboot/multiboot.html.)
 */

#ifndef MUTIBOOT_H_
#define MUTIBOOT_H_

/*
 * Define constant values across boot loaders
 */
#define MUTIBOOT_MAXCMDLINE   (4096)
#define MUTIBOOT_MAXMODNAME    (256)

/*
 * Mutiboot Header
 *
 * The Mutiboot Header must be 8-bytes aligned, and must fit entirely within the
 * first 8192 bytes of the lowest loaded ELF segment.
 */
#define MUTIBOOT_MAGIC             0x1BADB005 /* Mutiboot header signature */
#define MUTIBOOT_ALIGNMENT         8
#define MUTIBOOT_SEARCH            8192

/*
 * These flags expose OS requirements and support.
 *
 * Flag bits 0 - 15 indicate required features and the boot loader stops, if it
 * does not support all requirements. All non-specified features flags must be
 * 0. Flag 16 - 31 describes optional features. The boot loader continues, even
 * if it does not support all of the optional flags.
 */
#define MUTIBOOT_ARCH_FLAG_ARM64_EL1   (1 << 0)   /* Kernel runs in EL1, not EL2 */
#define MUTIBOOT_FLAG_VIDEO            (1 << 2)   /* Must pass video info to OS;
                                                     non-min video fields valid */
#define MUTIBOOT_FLAG_EFI_RTS_OLD      (1 << 17)  /* Reserved; do not redefine */
#define MUTIBOOT_FLAG_EFI_RTS          (1 << 18)  /* EFI RTS fields valid */
#define MUTIBOOT_FLAG_LOADESX_VERSION  (1 << 19)  /* LoadESX version field valid */
#define MUTIBOOT_FLAG_VIDEO_MIN        (1 << 20)  /* Video min fields valid */

#define MUTIBOOT_VIDEO_GRAPHIC         0          /* Linear graphics mode */
#define MUTIBOOT_VIDEO_TEXT            1          /* EGA-standard text mode */

typedef struct Mutiboot_Header {
   uint32_t magic;           /* Mutiboot Header Magic */
   uint32_t flags;           /* Feature flags */
   uint32_t checksum;        /* (magic + flags + checksum) sum up to zero */
   /*
    * The "reserved" fields were added by mistake from original Multiboot.
    * Never used in ESX or its bootloaders. These are prime candidates for
    * adding future functionality in a backward-compatible fashion.
    */
   uint32_t reserved[2];
   uint32_t min_width;       /* Video: minimum horizontal resolution */
   uint32_t min_height;      /* Video: minimum vertical resolution */
   uint32_t min_depth;       /* Video: minimum bits per pixel (0 for text) */
   uint32_t mode_type;       /* Video: preferred mode (0=graphic, 1=text) */
   uint32_t width;           /* Video: preferred horizontal resolution */
   uint32_t height;          /* Video: preferred vertical resolution */
   uint32_t depth;           /* Video: preferred bits per pixel (0 for text) */
   uint64_t rts_vaddr;       /* Virtual address of UEFI Runtime Services */
   uint64_t rts_size;        /* For new-style RTS. */
   uint32_t loadesx_version; /* LoadESX version supported */
} __attribute__((packed)) Mutiboot_Header;

/*
 * Mutiboot Info
 *
 * x86/x64:
 *  EAX - Mutiboot magic.
 *  EBX - PA of Mutiboot information data structure.
 *
 * ARM64:
 *  x0 - Mutiboot magic.
 *  x1 - PA of Mutiboot information data structure.
 *
 * The Mutiboot information data structure is used by the the boot
 * loader to communicate vital information to the operating system.
 * The operating system can use or ignore any parts of the structure
 * as it chooses; all information passed by the boot loader is advisory only.
 *
 * The Mutiboot information structure and its related substructures may be
 * placed anywhere in memory by the boot loader (with the exception of the
 * memory reserved for the kernel and boot modules, of course). It is the
 * operating system's responsibility to avoid overwriting this memory until it
 * is done using it.
 */
typedef enum Mutiboot_Type {
   MUTIBOOT_INVALID_TYPE,
   MUTIBOOT_MEMRANGE_TYPE,
   MUTIBOOT_MODULE_TYPE,
   MUTIBOOT_VBE_TYPE,
   MUTIBOOT_EFI_TYPE,
   MUTIBOOT_LOADESX_TYPE,
   MUTIBOOT_LOADESX_CHECKS_TYPE,
   NUM_MUTIBOOT_TYPE
} Mutiboot_Type;

typedef struct Mutiboot_Elmt {
   Mutiboot_Type type;
   uint64_t elmtSize;
} __attribute__((packed)) Mutiboot_Elmt;

typedef struct Mutiboot_MemRange {
   Mutiboot_Type type;
   uint64_t elmtSize;

   uint64_t startAddr;
   uint64_t len;
   uint32_t memType;
} __attribute__((packed)) Mutiboot_MemRange;

typedef struct Mutiboot_ModuleRange {
   uint64_t startPageNum;
   uint32_t numPages;
   uint32_t padding;
} __attribute__((packed)) Mutiboot_ModuleRange;

typedef struct Mutiboot_Module {
   Mutiboot_Type type;
   uint64_t elmtSize;

   uint64_t string;
   uint64_t moduleSize;

   uint32_t numRanges;
   Mutiboot_ModuleRange ranges[0];
} __attribute__((packed)) Mutiboot_Module;

/* VBE flags */
#define MUTIBOOT_VBE_FB64              (1UL << 0)  /* fbBaseAddress in use */

typedef struct Mutiboot_Vbe {
   Mutiboot_Type type;
   uint64_t elmtSize;

   uint64_t vbe_control_info; // main VBE header
   uint64_t vbe_mode_info;    // current mode definition
   uint16_t vbe_mode;         // current mode index
   uint32_t vbe_flags;        // Mutiboot VBE flags.
   /*
    * The fb base address field in the vbe_mode_info structure is only
    * 32-bit wide. If MUTIBOOT_VBE_FB64 is set in vbe_flags, then the
    * fb base address is stored in fbBaseAddress.
    */
   uint64_t fbBaseAddress;
} __attribute__((packed)) Mutiboot_Vbe;

/* EFI flags */
#define MUTIBOOT_EFI_ARCH64            (1<<0)  /* 64-bit EFI */
#define MUTIBOOT_EFI_SECURE_BOOT       (1<<1)  /* EFI Secure Boot in progress */
#define MUTIBOOT_EFI_MMAP              (1<<2)  /* EFI memory map is valid */

typedef struct Mutiboot_Efi {
   Mutiboot_Type type;
   uint64_t elmtSize;

   uint32_t efi_flags;
   uint64_t efi_systab;         // EFI system table physical address

   /* Set if efi_flags & MUTIBOOT_EFI_MMAP */
   uint64_t efi_mmap;           // EFI memory map physical address
   uint32_t efi_mmap_num_descs; // Number of descriptors in EFI memory map
   uint32_t efi_mmap_desc_size; // Size of each descriptor
   uint32_t efi_mmap_version;   // Descriptor version
} __attribute__((packed)) Mutiboot_Efi;

/* LoadESX Flags */
#define MUTIBOOT_LOADESX_ENABLE           (1<<0)  /* Enable LoadESX */
#define MUTIBOOT_LOADESX_IGNORE_PRECHECK  (1<<1)  /* Ignore Prechecks */
#define MUTIBOOT_LOADESX_USES_MEMXFERFS   (1<<2)  /* LoadESX uses MemXferFS */

typedef struct Mutiboot_LoadESX {
   Mutiboot_Type type;
   uint64_t elmtSize;

   uint64_t flags;
   /* Set if flags & MUTIBOOT_LOADESX_ENABLE */
   uint8_t enableLoadESX;
   /* Set if flags & MUTIBOOT_LOADESX_IGNORE_PRECHECK */
   uint8_t ignorePrecheck;
   /* Set if flags & MUTIBOOT_LOADESX_USES_MEMXFERFS */
   uint64_t memXferFsStartMPN;
} __attribute__((packed)) Mutiboot_LoadESX;

#define MUTIBOOT_LOADESX_CHECK_MAX_LEN 32
typedef struct Mutiboot_LoadESXCheck {
   char name[MUTIBOOT_LOADESX_CHECK_MAX_LEN];
   uint64_t cookie;
} Mutiboot_LoadESXCheck;

typedef struct Mutiboot_LoadESXChecks {
   Mutiboot_Type type;
   uint64_t elmtSize;

   uint8_t numLoadESXChecks;
   Mutiboot_LoadESXCheck loadESXChecks[0];
} __attribute__((packed)) Mutiboot_LoadESXChecks;

typedef struct Mutiboot_Info {
   uint64_t cmdline;

   uint64_t numMutibootElmt;
   Mutiboot_Elmt elmts[0];
} __attribute__((packed)) Mutiboot_Info;

#define FOR_EACH_MUTIBOOT_ELMT_DO(mbi, elmt)                        \
   do {                                                             \
      uint64_t __i;                                                 \
                                                                    \
      for (__i = 0,                                                 \
           elmt = (__typeof__(elmt)) &(mbi)->elmts[0];              \
           __i < (mbi)->numMutibootElmt;                            \
           __i += 1,                                                \
           elmt = (__typeof__(elmt))((uint8_t *)elmt + elmt->elmtSize)) { \

#define FOR_EACH_MUTIBOOT_ELMT_DONE(mbi, elmt)          \
      }                                                 \
      elmt = NULL;                                      \
   } while (0)


#define FOR_EACH_MUTIBOOT_ELMT_TYPE_DO(mbi, elmtType, elmt)     \
   FOR_EACH_MUTIBOOT_ELMT_DO(mbi, elmt) {                       \
      if (elmt->type == elmtType) {

#define FOR_EACH_MUTIBOOT_ELMT_TYPE_DONE(mbi, elmt)             \
      }                                                         \
   } FOR_EACH_MUTIBOOT_ELMT_DONE(mbi, elmt)


#endif /* MUTIBOOT_H_ */

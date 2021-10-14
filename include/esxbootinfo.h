/*******************************************************************************
 * Copyright (c) 2015-2021 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * esxbootinfo.h --
 *
 *       The ESXBootInfo boot loader interface is a redesign of the
 *       Multiboot interface(*).  It takes over some design concepts,
 *       but widens all address fields to 64 bits and omits obsolete
 *       features.
 *
 *       ESXBootInfo is built around a variable size array of elements,
 *       where each element has its own type and size.  The
 *       specification can easily be extended in the future in a
 *       compatible manner by adding more element types and sizes.
 *
 *       ESXBootInfo is architecture and platform-agnostic.
 *
 *       (* The Multiboot specification document can be found at
 *       http://www.gnu.org/software/grub/manual/multiboot/multiboot.html.)
 */

#ifndef ESXBOOTINFO_H_
#define ESXBOOTINFO_H_

/*
 * Define constant values across boot loaders
 */
#define ESXBOOTINFO_MAXCMDLINE   (4096)
#define ESXBOOTINFO_MAXMODNAME    (256)

/*
 * ESXBootInfo Header
 *
 * The ESXBootInfo Header must be 8-bytes aligned, and must fit entirely
 * within the first 8192 bytes of the lowest loaded ELF segment.
 */
#define ESXBOOTINFO_MAGIC             0x1BADB005 /* header signature */
#define ESXBOOTINFO_ALIGNMENT         8
#define ESXBOOTINFO_SEARCH            8192

/*
 * These flags expose OS requirements and support.
 *
 * Flag bits 0 - 15 indicate required features and the boot loader stops, if it
 * does not support all requirements. All non-specified features flags must be
 * 0. Flag 16 - 31 describes optional features. The boot loader continues, even
 * if it does not support all of the optional flags.
 */
#define ESXBOOTINFO_ARCH_FLAG_ARM64_EL1   (1 << 0)   /* Kernel runs in EL1, not EL2 */
#define ESXBOOTINFO_FLAG_VIDEO            (1 << 2)   /* Must pass video info to OS;
                                                        non-min video fields valid */
#define ESXBOOTINFO_ARCH_FLAG_ARM64_VHE   (1 << 16)  /* Kernel supports VHE */
#define ESXBOOTINFO_FLAG_EFI_RTS_OLD      (1 << 17)  /* Reserved; do not redefine */
#define ESXBOOTINFO_FLAG_EFI_RTS          (1 << 18)  /* EFI RTS fields valid */
#define ESXBOOTINFO_FLAG_LOADESX_VERSION  (1 << 19)  /* LoadESX version field valid */
#define ESXBOOTINFO_FLAG_VIDEO_MIN        (1 << 20)  /* Video min fields valid */
#define ESXBOOTINFO_FLAG_TPM_MEASUREMENT  (1 << 21)  /* TPM measurement field valid */

#define ESXBOOTINFO_VIDEO_GRAPHIC         0          /* Linear graphics mode */
#define ESXBOOTINFO_VIDEO_TEXT            1          /* EGA-standard text mode */

/*
 * These flags expose OS TPM measurement requests to the bootloader.
 *
 * The OS reports the measurement versions it supports, and the bootloader will
 * measure the highest version it supports from that set. The actual measured
 * version is reported through ESXBootInfo_Tpm.
 */
#define ESXBOOTINFO_TPM_MEASURE_NONE      0          /* No measurement */
#define ESXBOOTINFO_TPM_MEASURE_V1        (1 << 0)   /* Mods, cmdline, certs, tag. */

/*
 * ESXBootInfo_Header passed statically from kernel to bootloader.
 */
typedef struct ESXBootInfo_Header {
   uint32_t magic;           /* ESXBootInfo Header Magic */
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
   uint32_t tpm_measure;     /* TPM: determine what bootloader measures */
} __attribute__((packed)) ESXBootInfo_Header;

/*
 * ESXBootInfo passed from bootloader to kernel.
 *
 * x86/x64:
 *  EAX - ESXBootInfo magic.
 *  EBX - PA of ESXBootInfo data structure.
 *
 * ARM64:
 *  x0 - ESXBootInfo magic.
 *  x1 - PA of ESXBootInfo data structure.
 *
 * The ESXBootInfo data structure is used by the the boot loader to communicate
 * vital information to the operating system.  The operating system can use or
 * ignore any parts of the structure as it chooses; all information passed by
 * the boot loader is advisory only.
 *
 * The ESXBootInfo structure and its related substructures may be placed
 * anywhere in memory by the boot loader (with the exception of the memory
 * reserved for the kernel and boot modules, of course). It is the operating
 * system's responsibility to avoid overwriting this memory until it is done
 * using it.
 */
typedef enum ESXBootInfo_Type {
   ESXBOOTINFO_INVALID_TYPE,
   ESXBOOTINFO_MEMRANGE_TYPE,
   ESXBOOTINFO_MODULE_TYPE,
   ESXBOOTINFO_VBE_TYPE,
   ESXBOOTINFO_EFI_TYPE,
   ESXBOOTINFO_LOADESX_TYPE,
   ESXBOOTINFO_LOADESX_CHECKS_TYPE,
   ESXBOOTINFO_TPM_TYPE,
   NUM_ESXBOOTINFO_TYPE
} ESXBootInfo_Type;

typedef struct ESXBootInfo_Elmt {
   ESXBootInfo_Type type;
   uint64_t elmtSize;
} __attribute__((packed)) ESXBootInfo_Elmt;

typedef struct ESXBootInfo_MemRange {
   ESXBootInfo_Type type;
   uint64_t elmtSize;

   uint64_t startAddr;
   uint64_t len;
   uint32_t memType;
} __attribute__((packed)) ESXBootInfo_MemRange;

typedef struct ESXBootInfo_ModuleRange {
   uint64_t startPageNum;
   uint32_t numPages;
   uint32_t padding;
} __attribute__((packed)) ESXBootInfo_ModuleRange;

typedef struct ESXBootInfo_Module {
   ESXBootInfo_Type type;
   uint64_t elmtSize;

   uint64_t string;
   uint64_t moduleSize;

   uint32_t numRanges;
   ESXBootInfo_ModuleRange ranges[0];
} __attribute__((packed)) ESXBootInfo_Module;

/* VBE flags */
#define ESXBOOTINFO_VBE_FB64              (1UL << 0)  /* fbBaseAddress in use */

typedef struct ESXBootInfo_Vbe {
   ESXBootInfo_Type type;
   uint64_t elmtSize;

   uint64_t vbe_control_info; // main VBE header
   uint64_t vbe_mode_info;    // current mode definition
   uint16_t vbe_mode;         // current mode index
   uint32_t vbe_flags;        // ESXBootInfo VBE flags.
   /*
    * The fb base address field in the vbe_mode_info structure is only
    * 32-bit wide. If ESXBOOTINFO_VBE_FB64 is set in vbe_flags, then the
    * fb base address is stored in fbBaseAddress.
    */
   uint64_t fbBaseAddress;
} __attribute__((packed)) ESXBootInfo_Vbe;

/* EFI flags */
#define ESXBOOTINFO_EFI_ARCH64            (1<<0)  /* 64-bit EFI */
#define ESXBOOTINFO_EFI_SECURE_BOOT       (1<<1)  /* EFI Secure Boot in progress */
#define ESXBOOTINFO_EFI_MMAP              (1<<2)  /* EFI memory map is valid */

typedef struct ESXBootInfo_Efi {
   ESXBootInfo_Type type;
   uint64_t elmtSize;

   uint32_t efi_flags;
   uint64_t efi_systab;         // EFI system table physical address

   /* Set if efi_flags & ESXBOOTINFO_EFI_MMAP */
   uint64_t efi_mmap;           // EFI memory map physical address
   uint32_t efi_mmap_num_descs; // Number of descriptors in EFI memory map
   uint32_t efi_mmap_desc_size; // Size of each descriptor
   uint32_t efi_mmap_version;   // Descriptor version
} __attribute__((packed)) ESXBootInfo_Efi;

/* LoadESX Flags */
#define ESXBOOTINFO_LOADESX_USES_MEMXFERFS   (1<<2)  /* LoadESX uses MemXferFS */

typedef struct ESXBootInfo_LoadESX {
   ESXBootInfo_Type type;
   uint64_t elmtSize;

   uint64_t flags;
   /* Currently unused; set to 0. */
   uint16_t padding;
   /* Set if flags & ESXBOOTINFO_LOADESX_USES_MEMXFERFS */
   uint64_t memXferFsStartMPN;
} __attribute__((packed)) ESXBootInfo_LoadESX;

#define ESXBOOTINFO_LOADESX_CHECK_MAX_LEN 32
typedef struct ESXBootInfo_LoadESXCheck {
   char name[ESXBOOTINFO_LOADESX_CHECK_MAX_LEN];
   uint64_t cookie;
} ESXBootInfo_LoadESXCheck;

typedef struct ESXBootInfo_LoadESXChecks {
   ESXBootInfo_Type type;
   uint64_t elmtSize;

   uint8_t numLoadESXChecks;
   ESXBootInfo_LoadESXCheck loadESXChecks[0];
} __attribute__((packed)) ESXBootInfo_LoadESXChecks;

/* TPM Flags */
#define ESXBOOTINFO_TPM_EVENT_LOG_TRUNCATED     (1 << 0)
#define ESXBOOTINFO_TPM_EVENTS_MEASURED_V1      (1 << 1)

typedef struct ESXBootInfo_Tpm {
   ESXBootInfo_Type type;
   uint64_t elmtSize;

   uint32_t flags;

   uint32_t eventLogSize;
   uint8_t eventLog[0];
} __attribute__((packed)) ESXBootInfo_Tpm;

typedef struct ESXBootInfo {
   uint64_t cmdline;

   uint64_t numESXBootInfoElmt;
   ESXBootInfo_Elmt elmts[0];
} __attribute__((packed)) ESXBootInfo;

#define FOR_EACH_ESXBOOTINFO_ELMT_DO(ebi, elmt)                           \
   do {                                                                   \
      uint64_t __i;                                                       \
                                                                          \
      for (__i = 0,                                                       \
           elmt = (__typeof__(elmt)) &(ebi)->elmts[0];                    \
           __i < (ebi)->numESXBootInfoElmt;                               \
           __i += 1,                                                      \
           elmt = (__typeof__(elmt))((uint8_t *)elmt + elmt->elmtSize)) {

#define FOR_EACH_ESXBOOTINFO_ELMT_DONE(ebi, elmt)                         \
      }                                                                   \
      elmt = NULL;                                                        \
   } while (0)


#define FOR_EACH_ESXBOOTINFO_ELMT_TYPE_DO(ebi, elmtType, elmt)            \
   FOR_EACH_ESXBOOTINFO_ELMT_DO(ebi, elmt) {                              \
      if (elmt->type == elmtType) {

#define FOR_EACH_ESXBOOTINFO_ELMT_TYPE_DONE(ebi, elmt)                    \
      }                                                                   \
   } FOR_EACH_ESXBOOTINFO_ELMT_DONE(ebi, elmt)


#endif /* ESXBOOTINFO_H_ */

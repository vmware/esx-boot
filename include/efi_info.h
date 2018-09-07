/*******************************************************************************
 * Copyright (c) 2008-2018 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * efi_info.h --
 *
 *      boot_info_t field holding EFI information.
 */

#ifndef EFI_INFO_H_
#define EFI_INFO_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct {
   bool valid;                /* EFI info is valid? */
   bool secure_boot;          /* True if booting in Secure Boot mode */
   uint64_t systab;           /* EFI system table pointer */
   uint32_t systab_size;      /* System table size */
   void *mmap;                /* EFI_MEMORY_DESCRIPTOR structures */
   uint32_t num_descs;        /* Number of EFI memory descriptors */
   uint32_t desc_size;        /* Size of each EFI memory descriptor */
   uint32_t version;          /* Version of the EFI memory descriptors */
   uint64_t rts_vaddr;        /* Runtime services virtual address */
   uint64_t rts_size;         /* Size of the region for compressed mappings */

#define EFI_RTS_UNSUPPORTED   (1ULL << 0) /* Nothing we can do here */
#define EFI_RTS_OLD_AND_NEW   (1ULL << 1) /* Both set of mappings must be
                                             present for the call to
                                             SetVirtualAddressMap */
#define EFI_RTS_UNKNOWN_MEM   (1ULL << 2) /* UEFI SetVirtualAddressMap
                                             accesses ranges beyond
                                             those in the UEFI memory map */
#define EFI_FB_BROKEN         (1ULL << 3) /* Don't use UEFI framebuffer */
#define EFI_NET_DEV_DISABLE   (1ULL << 4) /*  Disconnect network drivers
                                              to work around running device DMA
                                              after ExitBootServices */
   uint64_t quirks;
#define EFI_RTS_CAP_OLD_AND_NEW   (1ULL << 0) /* We can create both sets
                                                 of mappings before call to
                                                 SetVirtualAddressMap */
#define EFI_RTS_CAP_RTS_DO_TEST   (1ULL << 1) /* RTS test, try actually
                                                 running RT code if
                                                 possible */
#define EFI_RTS_CAP_RTS_SIMPLE    (1ULL << 2) /* Can use simple policy. */
#define EFI_RTS_CAP_RTS_SIMPLE_GQ (1ULL << 3) /* Can use simple policy that
                                                 uses the generic pre/post
                                                 quirks to handle creating
                                                 both sets of mappings across
                                                 SetVirtualAddressMap */
#define EFI_RTS_CAP_RTS_SPARSE    (1ULL << 4) /* Can use sparse policy. */
#define EFI_RTS_CAP_RTS_COMPACT   (1ULL << 5) /* Can use compact policy. */
#define EFI_RTS_CAP_RTS_CONTIG    (1ULL << 6) /* Can use contig policy. */
   uint64_t caps;
} efi_info_t;

#endif

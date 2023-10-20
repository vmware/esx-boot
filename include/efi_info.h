/*******************************************************************************
 * Copyright (c) 2008-2018,2023 VMware, Inc.  All rights reserved.
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

#define EFI_BIT(n) ((uint64_t)1 << n)

typedef struct {
   bool valid;                /* EFI info is valid? */
   bool secure_boot;          /* True if booting in Secure Boot mode */
   bool use_memtype_sp;       /* Can use specific memory */
   uint64_t systab;           /* EFI system table pointer */
   uint32_t systab_size;      /* System table size */
   void *mmap;                /* EFI_MEMORY_DESCRIPTOR structures */
   uint32_t num_descs;        /* Number of EFI memory descriptors */
   uint32_t desc_size;        /* Size of each EFI memory descriptor */
   uint32_t version;          /* Version of the EFI memory descriptors */
   uint64_t rts_vaddr;        /* Runtime services virtual address */
   uint64_t rts_size;         /* Size of the region for compressed mappings */

#define EFI_RTS_UNSUPPORTED   EFI_BIT(0) /* Nothing we can do here */
#define EFI_RTS_OLD_AND_NEW   EFI_BIT(1) /* Both set of mappings must be
                                            present for the call to
                                            SetVirtualAddressMap */
#define EFI_RTS_UNKNOWN_MEM   EFI_BIT(2) /* UEFI SetVirtualAddressMap
                                            accesses ranges beyond
                                            those in the UEFI memory map */
#define EFI_FB_BROKEN         EFI_BIT(3) /* Don't use UEFI framebuffer */
#define EFI_NET_DEV_DISABLE   EFI_BIT(4) /* Disconnect network drivers
                                            to work around running device DMA
                                            after ExitBootServices */
   uint64_t quirks;
#define EFI_RTS_CAP_OLD_AND_NEW   EFI_BIT(0) /* We can create both sets
                                                of mappings before call to
                                                SetVirtualAddressMap */
#define EFI_RTS_CAP_RTS_DO_TEST   EFI_BIT(1) /* RTS test, try actually
                                                running RT code if
                                                possible */
#define EFI_RTS_CAP_RTS_SIMPLE    EFI_BIT(2) /* Can use simple policy. */
#define EFI_RTS_CAP_RTS_SIMPLE_GQ EFI_BIT(3) /* Can use simple policy that
                                                uses the generic pre/post
                                                quirks to handle creating
                                                both sets of mappings across
                                                SetVirtualAddressMap */
#define EFI_RTS_CAP_RTS_SPARSE    EFI_BIT(4) /* Can use sparse policy. */
#define EFI_RTS_CAP_RTS_COMPACT   EFI_BIT(5) /* Can use compact policy. */
#define EFI_RTS_CAP_RTS_CONTIG    EFI_BIT(6) /* Can use contig policy. */
   uint64_t caps;
} efi_info_t;

#endif

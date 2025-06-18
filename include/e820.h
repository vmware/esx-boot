/*******************************************************************************
 * Copyright (c) 2008-2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc.
 * and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * e820.h -- E820 memory map definitions
 */

#ifndef E820_H_
#define E820_H_

#include <sys/types.h>
#include <compat.h>

#pragma pack(1)
typedef struct {
   uint32_t lowAddr;      /* low 32 bits of base address */
   uint32_t highAddr;     /* high 32 bits of base address */
   uint32_t lowLen;       /* low 32 bits of length */
   uint32_t highLen;      /* high 32 bits of length */
   uint32_t type;         /* see below */
   uint32_t attributes;   /* ACPI extended attributes */
} e820_range_t;
#pragma pack()

/* ACPI Memory types */
#define E820_TYPE_AVAILABLE   1  /* This range is available RAM usable by the
                                    operating system. */

#define E820_TYPE_RESERVED    2  /* This range of addresses is in use or
                                    reserved by the system and is not to be
                                    included in the allocatable memory pool of
                                    the operating system's memory manager. */

#define E820_TYPE_ACPI        3  /* ACPI Reclaim Memory. This range is available
                                    RAM usable by the OS after it reads the ACPI
                                    tables. */

#define E820_TYPE_ACPI_NVS    4  /* ACPI NVS Memory. This range of addresses is
                                    in use or reserve by the system and must not
                                    be used by the operating system. This range
                                    is required to be saved and restored across
                                    an NVS sleep. */

#define E820_TYPE_UNUSABLE    5  /* This range of addresses contains memory in
                                    which errors have been detected. This range
                                    must not be used by OSPM. Ranges of this
                                    type are converted to E820_TYPE_RESERVED. */

#define E820_TYPE_DISABLED    6  /* This range of addresses contains memory that
                                    is not enabled. This range must not be used
                                    by OSPM. Ranges of this type are converted
                                    to E820_TYPE_RESERVED. */

#define E820_TYPE_PMEM        7  /* PersistentMemory. This range information is
                                    is defined by platform ACPI tables and OS
                                    must parse those in order to use this range.
                                    For OS that doesn't have the support must
                                    treat this range as reserved memory. */

/*
 * Additional VMware-specific memory types for EFI runtime services
 * memory.  Used only if the multiboot header indicates the kernel
 * supports this feature.
 */
#define E820_TYPE_RTS_CODE          100
#define E820_TYPE_RTS_DATA          101
#define E820_TYPE_RTS_MMIO          102

/*
 * Additional memory types (for bootloader internal usage).
 */


/* E820_TYPE_BOOTLOADER contains the bootloader's memory. Ranges of this type
 * are converted to E820_TYPE_AVAILABLE prior to passing the memory map to the
 * operating system. Ranges of this type are fair game for relocating loaded
 * objects and allocated bootloader structures into as part of handing off to
 * the loaded OS. Ranges of this type must be used for memory that is allocated
 * by alloc and then used immediately. So they are blacklisted by
 * blacklist_bootloader_mem prior to the first such allocation.
 */
#define E820_TYPE_BOOTLOADER  0xffffffff

/*
 * E820 extended attributes that we care about
 */
#define E820_ATTR_ENABLED 0x1

#define E820_BASE(_entry_)                                              \
   uint32_concat((_entry_)->highAddr, (_entry_)->lowAddr)

#define E820_LENGTH(_entry_)                                            \
   uint32_concat((_entry_)->highLen, (_entry_)->lowLen)

static INLINE void e820_set_entry(e820_range_t *range, uint64_t base,
                                  uint64_t len, uint32_t type,
                                  uint32_t attributes)
{
   range->lowAddr = lowhalf64(base);
   range->highAddr = highhalf64(base);
   range->lowLen = lowhalf64(len);
   range->highLen = highhalf64(len);
   range->type = type;
   range->attributes = attributes;
}

#endif /* !E820_H_ */

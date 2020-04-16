/*******************************************************************************
 * Copyright (c) 2008-2011,2015-2016 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * cpu.h -- CPU-specific definitions
 */

#ifndef CPU_H_
#define CPU_H_

#include <compat.h>
#include <sys/types.h>
#include <stdbool.h>

/*
 * Interrupts
 */
#define EFLAGS_CF       0x00000001

static INLINE void CLI(void)
{
   __asm__ __volatile__ ("cli");
}

static INLINE void STI(void)
{
   __asm__ __volatile__ ("sti");
}

static INLINE void HLT(void)
{
   __asm__ __volatile__("hlt");
}

/*
 * Control registers
 */
#define CR0_ATTR_PG (1 << 31)

static INLINE void get_cr0(uintptr_t *cr0)
{
   __asm__ __volatile__("mov %%cr0, %0"
                        : "=r" (*cr0));
}

/*
 * Paging
 */
#define PAGE_SIZE  0x1000ULL
#define PG_TABLE_MAX_ENTRIES 512

#define PG_LEVEL_SHIFT       9
#define PG_MPN_SHIFT         12
/*
 * Bytes covered by an LnPTE.
 */
#define PG_TABLE_LnE_SIZE(n) ((uint64_t)1 << \
                              (PG_MPN_SHIFT + ((n) - 1) * PG_LEVEL_SHIFT))

#define PG_OFF_MASK            ((1 << PG_LEVEL_SHIFT) - 1)
#define PG_LPN_2_LnOFF(lpn, n) (((lpn) >> (PG_LEVEL_SHIFT * (n - 1))) & PG_OFF_MASK)
#define PG_LPN_2_L1OFF(lpn)     PG_LPN_2_LnOFF(lpn, 1)

#define PG_GET_ENTRY(pt, n, lpn) pt[PG_LPN_2_LnOFF(lpn, n)]

#define PG_SET_ENTRY_RAW(pt, n, value) do {                             \
      pt[(n)] = (value);                                                \
} while (0)

#define PG_SET_ENTRY(pt, n, lpn, mpn, flags)                            \
   PG_SET_ENTRY_RAW(pt, PG_LPN_2_LnOFF(lpn, n), (((mpn) << PG_MPN_SHIFT) | (flags)))

#define PG_ATTR_PRESENT    ((uint64_t)1 << 0)
#define PG_ATTR_W          ((uint64_t)1 << 1)
#define PG_ATTR_RO         (0)
#define PG_ATTR_PWT        ((uint64_t)1 << 3)
#define PG_ATTR_PCD        ((uint64_t)1 << 4)
#define PG_ATTR_A          ((uint64_t)1 << 5)
#define PG_ATTR_PAGE_SIZE  ((uint64_t)1 << 7)
#define PG_ATTR_PAT        ((uint64_t)1 << 7)
#define PG_ATTR_LARGE_PAT  ((uint64_t)1 << 12)
#define PG_ATTR_XD         ((uint64_t)1 << 63)
#define PG_ATTR_MASK       (PG_ATTR_XD | (uint64_t)0xfff)
#define PG_ATTR_LARGE_MASK (PG_ATTR_MASK | PG_ATTR_LARGE_PAT)
#define PG_FRAME_MASK      ((uint64_t)0xffffffffff000)

#define PG_DIR_CACHING_FLAGS(cr3) (cr3 & (PG_ATTR_PWT | PG_ATTR_PCD))

#define PG_IS_LARGE(entry) ((entry & PG_ATTR_PAGE_SIZE) != 0)
#define PG_IS_READONLY(entry) ((entry & PG_ATTR_W) == 0)
#define PG_ENTRY_TO_PG(entry) ((uint64_t *) (entry & PG_FRAME_MASK))

static inline uint64_t PG_ENTRY_TO_PAGE_FLAGS(uint64_t entry)
{
   uint64_t flags;

   if (PG_IS_LARGE(entry)) {
      /*
       * Need to convert large to small page flags. As per
       * Intel Vol. 3A 4-28 the only difference is the
       * location of the PAT flag.
       */
      flags = entry & (PG_ATTR_LARGE_MASK | ~PG_ATTR_PAGE_SIZE);
      if (flags & PG_ATTR_LARGE_PAT) {
         flags |= PG_ATTR_PAT;
      }
   } else {
      flags = entry & PG_ATTR_MASK;
   }

   return flags;
}

static INLINE void get_page_table_reg(uintptr_t *reg)
{
   __asm__ __volatile__("mov %%cr3, %0"
                        : "=r" (*reg));
}

static INLINE void *get_page_table_root(void)
{
   uintptr_t reg;

   get_page_table_reg(&reg);
   return (void *)(reg & ~(((uintptr_t)PAGE_SIZE) - 1));
}

static INLINE void set_page_table_reg(uintptr_t *reg)
{
   __asm__ __volatile__ ("mov %0, %%cr3"
                         :
                         : "r" (*reg)
                         : "memory");
}

static INLINE uint64_t get_page_table_mask(void)
{
   unsigned int eax, ebx;

   /*
    * If SEV is supported we must mask off the memory encryption bit when
    * mapping the PTEs.
    */
   __asm__ volatile("cpuid" :
                    "=a"(eax) : "0"(0x80000000) : "ebx", "ecx", "edx");
   __asm__ volatile("cpuid" :
                    "=a"(eax) : "0"(0x80000000) : "ebx", "ecx", "edx");
   if (eax >= 0x8000001F) {
      __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx) :
                             "0"(0x8000001F) :
                             "ecx", "edx");
      if ((eax & 2) != 0) {
         return (1ull << (ebx & 0x3F)) | PG_ATTR_MASK;
      }
   }
   return PG_ATTR_MASK;
}

static INLINE bool is_paging_enabled(void)
{
   uintptr_t cr0;

   get_cr0(&cr0);
   return ((cr0 & CR0_ATTR_PG) != 0);
}

static INLINE bool mmu_supported_configuration(void)
{
   return true;
}

/*-- cpu_code_update -----------------------------------------------------------
 *
 *       Cache coherence when code is written prior to execution.
 *
 *       Needs to be always inline as is called from trampoline code, and
 *       must be relocation-safe.
 *
 * Parameters
 *      IN va:    buffer with code
 *      IN len:   length of buffer
 *----------------------------------------------------------------------------*/
static ALWAYS_INLINE void cpu_code_update(UNUSED_PARAM(uintptr_t va),
                                          UNUSED_PARAM(uint64_t len))
{
   /* Nothing to do here. */
}

/*-- cpu_code_update_commit ----------------------------------------------------
 *
 *       Finish a sequence of cache coherence operations when code is written
 *       prior to execution.
 *
 *       Needs to be always inline as is called from trampoline code, and
 *       must be relocation-safe.
 *
 *----------------------------------------------------------------------------*/
static ALWAYS_INLINE void cpu_code_update_commit(void)
{
   /* Nothing to do here. */
}

#endif /* !CPU_H_ */

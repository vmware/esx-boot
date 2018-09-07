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

#define SCTLR_MMU                         (1ULL << 0)
#define TCR_ELx_TG0_SHIFT                 (14)
#define TCR_ELx_TG0_MASK                  (3UL)
#define TCR_GRANULARITY_4K                (0)
#define TCR_ELx_T0SZ_MASK                 (0x3FUL)
#define TCR_ELx_T0SZ_MAX                  16
#define TCR_ELx_T0SZ_MIN_WITH_PML4_LOOKUP 24


/*
 * Cache Type Register (CTR) line sizes, see D7.2.21 [ARMv8-ARM].
 */
#define ARM_CTR_LINE_MASK        0xf
#define ARM_CTR_IMINLINE_SHIFT   0
#define ARM_CTR_DMINLINE_SHIFT   16

/*
 * Barriers.
 */
#define rmb() __asm__ __volatile__("dsb ld" : : : "memory")
#define wmb() __asm__ __volatile__("dsb st" : : : "memory")
#define ISB() __asm__ __volatile__("isb" : : : "memory")
#define DSB() __asm__ __volatile__("dsb sy" : : : "memory")

/*
 * Interrupts.
 */
static INLINE void CLI(void)
{
   __asm__ __volatile__ ("msr daifset, #2");

}

static INLINE void STI(void)
{
   __asm__ __volatile__ ("msr daifclr, #2");
}

/*
 * MSR register accesses.
 */
#define MRS(where, regname) __asm__ __volatile__("mrs %0, " STRINGIFY(regname) : "=r" (where))
#define MSR(regname, what)  __asm__ __volatile__("msr " STRINGIFY(regname) ", %0" : : "r" (what))

#define PSR_M_EL_SHIFT 2
#define PSR_M_EL_MASK  0x3

static INLINE bool el_is_hyp(void)
{
   uint64_t el;

   MRS(el, CurrentEL);
   el >>= PSR_M_EL_SHIFT;
   el &= PSR_M_EL_MASK;

   return el == 2;
}

/*
 * Paging
 */
#define PAGE_SIZE 0x1000ULL
#define PG_TABLE_MAX_ENTRIES 512

#define PG_LEVEL_SHIFT       9
#define PG_MPN_SHIFT         12
#define PG_LPN_SHIFT         12
/*
 * Bytes covered by an LnPTE.
 */
#define PG_TABLE_LnE_SIZE(n) (1ULL << (PG_MPN_SHIFT + ((n) - 1) * PG_LEVEL_SHIFT))

#define PG_OFF_MASK            ((1 << PG_LEVEL_SHIFT) - 1)
#define PG_LPN_2_LnOFF(lpn, n) (((lpn) >> (PG_LEVEL_SHIFT * (n - 1))) & PG_OFF_MASK)
#define PG_LPN_2_L1OFF(lpn)     PG_LPN_2_LnOFF(lpn, 1)

#define PG_GET_ENTRY(pt, n, lpn) pt[PG_LPN_2_LnOFF(lpn, n)]

#define PG_SET_ENTRY_RAW(pt, n, value) do {                             \
      pt[(n)] = (value);                                                \
      __asm__ volatile("dc cvau, %0\n\t"                                \
                       "dsb sy     \n\t"                                \
                       "isb        \n\t"                                \
                       : : "r" (&pt[(n)]) : "memory");                  \
   } while (0)

#define PG_SET_ENTRY(pt, n, lpn, mpn, flags)                            \
   PG_SET_ENTRY_RAW(pt, PG_LPN_2_LnOFF(lpn, n), (((mpn) << PG_MPN_SHIFT) | (flags)))

#define PG_ATTR_PRESENT    (1ULL << 0)
#define PG_ATTR_W          (0)          // ARM uses RO bit.
#define PG_ATTR_RO         (1ULL << 7)
#define PG_ATTR_A          (1ULL << 10)
#define PG_ATTR_TABLE      (1ULL << 2)
#define PG_ATTR_MASK       (0xf870000000000fffULL)
#define PG_FRAME_MASK      (0xffffffffff000ULL)

#define PG_DIR_CACHING_FLAGS(ttbr0) (0)
#define PG_IS_LARGE(entry) ((entry & PG_ATTR_TABLE) == 0)
#define PG_IS_READONLY(entry) ((entry & PG_ATTR_RO) != 0)
#define PG_ENTRY_TO_PG(entry) ((uint64_t *) (entry & PG_FRAME_MASK))

static inline uint64_t PG_ENTRY_TO_PAGE_FLAGS(uint64_t entry)
{
   return entry & PG_ATTR_MASK;
}

/*
 * MMU accessors.
 */
static INLINE void get_page_table_reg(uintptr_t *reg)
{
   if (el_is_hyp()) {
      MRS(*reg, ttbr0_el2);
   } else {
      MRS(*reg, ttbr0_el1);
   }
}

static INLINE void *get_page_table_root(void)
{
   uintptr_t reg;

   get_page_table_reg(&reg);
   return (void *) reg;
}

static INLINE void tlbi_all(void)
{
   if (el_is_hyp()) {
      __asm__ volatile("tlbi alle2\n\t" : : : "memory");
   } else {
      __asm__ volatile("tlbi vmalle1\n\t" : : : "memory");
   }

   DSB();
   ISB();
}

static INLINE void set_page_table_reg(uintptr_t *reg)
{
   if (el_is_hyp()) {
      MSR(ttbr0_el2, *reg);
   } else {
      MSR(ttbr0_el1, *reg);
   }
   ISB();
   tlbi_all();
}

static INLINE uint64_t get_page_table_mask(void)
{
   return PG_ATTR_MASK;
}

static INLINE bool is_paging_enabled(void)
{
   uintptr_t sctlr;

   if (el_is_hyp()) {
      MRS(sctlr, sctlr_el2);
   } else {
      MRS(sctlr, sctlr_el1);
   }
   return sctlr & SCTLR_MMU;
}

static INLINE int mmu_t0sz(void)
{
   int t0sz;
   uint64_t tcr;

   if (el_is_hyp()) {
      MRS(tcr, tcr_el2);
   } else {
      MRS(tcr, tcr_el1);
   }
   t0sz = tcr & TCR_ELx_T0SZ_MASK;
   if (t0sz < TCR_ELx_T0SZ_MAX) {
      t0sz = TCR_ELx_T0SZ_MAX;
   }

   return t0sz;
}

static INLINE bool mmu_supported_configuration(void)
{
   int i;
   int gran;
   int t0sz;
   uint64_t *l4pt;
   uint64_t tcr;
   uint64_t cur_max_va;

   if (!is_paging_enabled()) {
      return false;
   }

   if (el_is_hyp()) {
      MRS(tcr, tcr_el2);
   } else {
      MRS(tcr, tcr_el1);
   }
   gran = (tcr >> TCR_ELx_TG0_SHIFT) & TCR_ELx_TG0_MASK;
   if (gran != TCR_GRANULARITY_4K) {
      /*
       * Not supposed to happen according to the UEFI
       * spec, but that has never stopped anyone before.
       */
      return false;
   }

   t0sz = mmu_t0sz();
   if (t0sz > TCR_ELx_T0SZ_MIN_WITH_PML4_LOOKUP) {
      /*
       * The VA region is so small we don't even *have*
       * a PML4.
       */
      return false;
   }

   if (t0sz == TCR_ELx_T0SZ_MAX) {
      return true;
   }

   /*
    * Firmware set a limited VA range, but fortunately
    * this is pretty easy to work around: make sure
    * the PML4 doesn't contain garbage for the missing
    * entries and increase the VA range. This only works
    * because UEFI allocates *pages*, so even though
    * a reduced VA range also implies the PML4 is
    * smaller, we can treat it as a full table
    * without fear of clobbering something important.
    * (This really isn't as hacky as it seems... all the
    * other levels need to be page aligned).
    */
   cur_max_va = 1UL << (64 - t0sz);
   l4pt = get_page_table_root();
   if (((uintptr_t) l4pt & PG_OFF_MASK) != 0) {
      /*
       * Well the firmware tried to be too smart and
       * the PML4 doesn't have 4K alignment (and
       * allocation size, then).
       */
      return false;
   }

   for (i = PG_LPN_2_LnOFF(cur_max_va >> PG_LPN_SHIFT, 4);
        i < PG_TABLE_MAX_ENTRIES;
        i++) {
      PG_SET_ENTRY_RAW(l4pt, i, 0);
   }

   DSB();
   tcr &= ~TCR_ELx_T0SZ_MASK;
   tcr |= TCR_ELx_T0SZ_MAX;
   if (el_is_hyp()) {
      MSR(tcr_el2, tcr);
   } else {
      MSR(tcr_el1, tcr);
   }
   ISB();
   tlbi_all();

   return true;
}

/*-- cpu_code_update -----------------------------------------------------------
 *
 *       Cache coherence when code is written prior to execution.
 *
 *       Needs to be always inline as is called from trampoline code, and
 *       must be relocation-safe.
 *
 *       Because we might be running on CPUs with PIPT, VIPT or AIVIVT
 *       I-caches, we'll simply do D-cache maintenance to PoU and rely
 *       on cpu_code_update_commit to do a global I-cache invalidate.
 *
 * Parameters
 *      IN va:    buffer with code
 *      IN len:   length of buffer
 *----------------------------------------------------------------------------*/
static ALWAYS_INLINE void cpu_code_update(uintptr_t va, uint64_t len)
{
   uint64_t ctr;
   uint64_t dva;
   uint32_t dminline;

   /*
    * First we decode the CTR to determine the smallest D/I cache line sizes. We
    * need to clean/invalidate at this granularity to ensure we're hitting every
    * cache line on the way down to PoU.
    */
   MRS(ctr, ctr_el0);

   dminline = 1 << ((ctr >> ARM_CTR_DMINLINE_SHIFT) & ARM_CTR_LINE_MASK);

   /*
    * Clean D-cache to PoU.
    */
   for (dva = ROUNDDOWN(va, dminline);
        dva < va + len;
        dva += dminline) {
      __asm__ __volatile__("dc cvau, %0\n\t" :: "r" (dva) : "memory");
   }

   /* Ensure completion of clean. */
   DSB();
}

/*-- cpu_code_update_commit ----------------------------------------------------
 *
 *       Finish a sequence of cache coherence operations when code is written
 *       prior to execution.
 *
 *       Needs to be always inline as is called from trampoline code, and
 *       must be relocation-safe.
 *
 *       Because we might be running on CPUs with PIPT, VIPT or AIVIVT
 *       I-caches, cpu_code_update_commit always does a global I-cache
 *       invalidate.
 *
 *----------------------------------------------------------------------------*/
static ALWAYS_INLINE void cpu_code_update_commit(void)
{
   __asm__ __volatile__("ic iallu\n\t");
   DSB();
   ISB();
}

#endif /* !CPU_H_ */

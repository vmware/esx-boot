/*******************************************************************************
 * Copyright (c) 2008-2011,2015-2016,2021 VMware, Inc.  All rights reserved.
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

#define DAIF_F                            (1 << 0)
#define DAIF_I                            (1 << 1)
#define DAIF_A                            (1 << 2)

#define SCTLR_MMU                         ((uint64_t)1 << 0)
#define TCR_ELx_TG0_SHIFT                 (14)
#define TCR_ELx_TG0_MASK                  (3UL)
#define TCR_GRANULARITY_4K                (0)
#define TCR_ELx_TnSZ_MASK                 (0x3FUL)
#define TCR_ELx_TnSZ_MIN_WITH_PML4_LOOKUP 16
#define TCR_ELx_TnSZ_MAX_WITH_PML4_LOOKUP 24
#define TCR_ELx_TnSZ_MIN_WITH_PML3_LOOKUP 25
#define TCR_ELx_TnSZ_MAX_WITH_PML3_LOOKUP 33
#define TCR_ELx_TnSZ_MIN_WITH_PML2_LOOKUP 34
#define TCR_ELx_TnSZ_MAX_WITH_PML2_LOOKUP 39
#define HCR_E2H                           ((uint64_t)1 << 34)
#define MMFR1_VH_MASK                     (0xF00)
#define MMFR1_VH_NOT_PRESENT              (0)
#define PAR_EL1_ATTRS_SHIFT               (56)
#define PAR_EL1_ATTRS_MASK                (0xFF)
#define PAR_EL1_FLAGS_SHIFT               (0)
#define PAR_EL1_FLAGS_MASK                (0xFFF)

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
   __asm__ __volatile__ ("msr daifset, %0" :: "i" (DAIF_A | DAIF_I | DAIF_F));
}

static INLINE void STI(void)
{
   __asm__ __volatile__ ("msr daifclr, %0" :: "i" (DAIF_A | DAIF_I | DAIF_F));
}

static INLINE void HLT(void)
{
   /*
    * wfe instead of wfi to mimic x86 behavior on HLT after CLI.
    *
    * wfi doesn't care about masked interrupts.
    */
   __asm__ __volatile__ ("wfe");
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
#define PAGE_SIZE ((uint64_t)0x1000)
#define PG_TABLE_MAX_ENTRIES 512
#define PG_TABLE_MAX_LEVELS 4

#define PG_LEVEL_SHIFT       9
#define PG_MPN_SHIFT         12
#define PG_LPN_SHIFT         12
/*
 * Bytes covered by an LnPTE. The interface only makes sense if
 * level n corresponds to is <= max page table used.
 */
#define PG_TABLE_LnE_SIZE(n) ((uint64_t)1 << \
                              (PG_MPN_SHIFT + ((n) - 1) * PG_LEVEL_SHIFT))

#define PG_OFF_MASK            ((1 << PG_LEVEL_SHIFT) - 1)
#define PG_LPN_2_LnOFF(lpn, n) (((lpn) >> (PG_LEVEL_SHIFT * (n - 1))) & PG_OFF_MASK)
#define PG_LPN_2_L1OFF(lpn)     PG_LPN_2_LnOFF(lpn, 1)

#define PG_GET_ENTRY(pt, n, lpn) pt[PG_LPN_2_LnOFF(lpn, n)]

#define PG_SET_ENTRY_RAW(pt, n, value) do {                             \
      pt[(n)] = (value);                                                \
      DSB();                                                            \
      __asm__ volatile("dc cvau, %0\n\t"                                \
                       "dc cvac, %0\n\t"                                \
                       "dsb sy     \n\t"                                \
                       "isb        \n\t"                                \
                       : : "r" (&pt[(n)]) : "memory");                  \
   } while (0)

#define PG_SET_ENTRY(pt, n, lpn, mpn, flags)                            \
   PG_SET_ENTRY_RAW(pt, PG_LPN_2_LnOFF(lpn, n), (((mpn) << PG_MPN_SHIFT) | (flags)))

#define PG_ATTR_PRESENT    ((uint64_t)1 << 0)
#define PG_ATTR_W          (0)          // ARM uses RO bit.
#define PG_ATTR_RO         ((uint64_t)1 << 7)
#define PG_ATTR_EL0        ((uint64_t)1 << 6)
#define PG_ATTR_TABLE_RO   ((uint64_t)1 << 62)
#define PG_ATTR_TABLE_EL0  ((uint64_t)1 << 61)
#define PG_ATTR_A          ((uint64_t)1 << 10)
#define PG_ATTR_XN         ((uint64_t)1 << 54) /* EL 2 */
#define PG_ATTR_PXN        ((uint64_t)1 << 53) /* EL 1 */
#define PG_ATTR_XD         (PG_ATTR_XN | PG_ATTR_PXN)
#define PG_ATTR_TABLE_XN   ((uint64_t)1 << 60) /* EL 2 */
#define PG_ATTR_TABLE_PXN  ((uint64_t)1 << 59) /* EL 1 */
#define PG_ATTR_TABLE_XD   (PG_ATTR_TABLE_XN | PG_ATTR_TABLE_PXN)
#define PG_ATTR_TYPE_MASK  0x3
#define PG_ATTR_TYPE_BLOCK 0x1
#define PG_ATTR_TYPE_TABLE 0x3 /* When level > 1  */
#define PG_ATTR_TYPE_PAGE  0x3 /* When level == 1 */
#define PG_ATTR_MASK       ((uint64_t)0xFFF0000000000FFF)
#define PG_ATTR_LARGE_MASK PG_ATTR_MASK
#define PG_FRAME_MASK      ((uint64_t)0xFFFFFFFFF000)
/*
 * 48 bits. Higher bits are RES0 in EL2 and ASID in EL1.
 */
#define PG_ROOT_ADDR_MASK  ((uint64_t)0xFFFFFFFFFFFF)

#define PG_DIR_CACHING_FLAGS(ttbr0) (0)
#define PG_IS_LARGE(level, entry) ((level) != 1 && \
                                   (((entry) & PG_ATTR_TYPE_MASK) == \
                                    PG_ATTR_TYPE_BLOCK))
#define PG_IS_READONLY(entry) ((entry & PG_ATTR_RO) != 0)
#define PG_ENTRY_TO_PG(entry) ((uint64_t *) (entry & PG_FRAME_MASK))
/*
 * Arm ARM says:
 *
 *    For a translation regime that applies to EL0 and a
 *    higher Exception level, if the value of the AP[2:1] bits is
 *    0b01, permitting write access from EL0, then the PXN bit is
 *    treated as if it has the value 1, regardless of its actual
 *    value.
 *
 * Basically: if you clear RO, EL0 bit better be clear too, unless
 * you really like taking instruction aborts.
 */
#define PG_CLEAN_READONLY(entry) ((entry) & ~(PG_ATTR_RO | PG_ATTR_EL0))
#define PG_CLEAN_TABLE_READONLY(entry) ((entry) & ~(PG_ATTR_TABLE_RO | PG_ATTR_TABLE_EL0))
#define PG_CLEAN_NOEXEC(entry) ((entry) & ~PG_ATTR_XD)
#define PG_CLEAN_TABLE_NOEXEC(entry) ((entry) & ~PG_ATTR_TABLE_XD)
/*
 * Convert hierarchical table RO and XN bits into page
 * mapping (small or large) bits.
 */
#define PG_TABLE_XD_RO_2_PAGE_ATTRS(entry)               \
   ((((entry) & PG_ATTR_TABLE_XN) ? PG_ATTR_XN : 0)    | \
    (((entry) & PG_ATTR_TABLE_PXN) ? PG_ATTR_PXN : 0)  | \
    (((entry) & PG_ATTR_TABLE_RO) ? PG_ATTR_RO : 0)    | \
    (((entry) & PG_ATTR_TABLE_EL0) ? PG_ATTR_EL0 : 0))

static inline uint64_t PG_ENTRY_TO_PAGE_FLAGS(UNUSED_PARAM(unsigned level),
                                              uint64_t entry)
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
   return (void *)(reg & PG_ROOT_ADDR_MASK);
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

static INLINE uint64_t xlate_va_2_par(uintptr_t va)
{
   uint64_t par;

   CLI();

   if (el_is_hyp()) {
      __asm__ volatile("at s1e2r, %0" : : "r" (va));
   } else {
      __asm__ volatile("at s1e1r, %0" : : "r" (va));
   }

   ISB();

   MRS(par, par_el1);

   /*
    * On At least one system (X-Gene) we've seen invalid PAR_EL1
    * contents unless there was an ISB() following the read.
    */
   ISB();
   STI();

   return par;
}

static INLINE uint64_t xlate_va_2_flags(uintptr_t va)
{
   return (xlate_va_2_par(va) >> PAR_EL1_FLAGS_SHIFT) &
      PAR_EL1_FLAGS_MASK;
}

static INLINE uint64_t xlate_va_2_attrs(uintptr_t va)
{
   return (xlate_va_2_par(va) >> PAR_EL1_ATTRS_SHIFT) &
      PAR_EL1_ATTRS_MASK;
}

static INLINE void set_page_table_reg(uintptr_t *reg)
{
   DSB();
   ISB();

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

static INLINE uint64_t get_mair(void)
{
   uint64_t mair;

   if (el_is_hyp()) {
      MRS(mair, mair_el2);
   } else {
      MRS(mair, mair_el1);
   }

   return mair;
}

static INLINE uint64_t get_sctlr(void)
{
   uintptr_t sctlr;

   if (el_is_hyp()) {
      MRS(sctlr, sctlr_el2);
   } else {
      MRS(sctlr, sctlr_el1);
   }

   return sctlr;
}

static INLINE bool is_paging_enabled(void)
{
   uintptr_t sctlr = get_sctlr();

   return sctlr & SCTLR_MMU;
}

static INLINE uint64_t get_tcr(void)
{
   uint64_t tcr;

   if (el_is_hyp()) {
      MRS(tcr, tcr_el2);
   } else {
      MRS(tcr, tcr_el1);
   }

   return tcr;
}

static INLINE unsigned mmu_t0sz(void)
{
   unsigned t0sz;
   uint64_t tcr = get_tcr();

   t0sz = tcr & TCR_ELx_TnSZ_MASK;
   if (t0sz < TCR_ELx_TnSZ_MIN_WITH_PML4_LOOKUP) {
      t0sz = TCR_ELx_TnSZ_MIN_WITH_PML4_LOOKUP;
   }

   return t0sz;
}

static INLINE unsigned mmu_max_levels(void)
{
   unsigned t0sz;

   t0sz = mmu_t0sz();
   if (t0sz >= TCR_ELx_TnSZ_MIN_WITH_PML4_LOOKUP &&
       t0sz <= TCR_ELx_TnSZ_MAX_WITH_PML4_LOOKUP) {
      return 4;
   } else if (t0sz >= TCR_ELx_TnSZ_MIN_WITH_PML3_LOOKUP &&
              t0sz <= TCR_ELx_TnSZ_MAX_WITH_PML3_LOOKUP) {
      return 3;
   } else if (t0sz >= TCR_ELx_TnSZ_MIN_WITH_PML2_LOOKUP &&
              t0sz <= TCR_ELx_TnSZ_MAX_WITH_PML2_LOOKUP) {
      return 2;
   }

   return 0;
}

static INLINE unsigned mmu_max_entries(int level)
{
   int x;
   int y;
   int bits;
   int t0sz;
   int max_level;

   max_level = mmu_max_levels();
   if (level > max_level || level == 0) {
     return 0;
   }

   /*
    * This follows the logic from Table D4-25 "Translation table
    * entry addresses when using the 4KB translation granule" from
    * ARM DDI 0487A.k.
    *
    * It looks weird, because you may have used less than 4 page table
    * levels, depending on the size of the input (VA) address range. The size
    * of the input address range used by UEFI depends on how much RAM
    * is being seen by UEFI.
    */
   t0sz = mmu_t0sz();
   switch (level) {
   case 4:
      /*
       * Can only get here if max_level is 4.
       */
      x = 28 - t0sz;
      y = x + 35;
      bits = y - 39 + 1;
      break;
   case 3:
      if (level == max_level) {
         x = 37 - t0sz;
      } else {
         x = 12;
      }
      y = x + 26;
      bits = y - 30 + 1;
      break;
   case 2:
      if (level == max_level) {
         x = 46 - t0sz;
      } else {
         x = 12;
      }
      y = x + 17;
      bits = y - 21 + 1;
      break;
   case 1:
   default:
      bits = 9;
      break;
   }

   return 1 << bits;
}

static INLINE uint64_t get_mmfr1(void)
{
   uint64_t mmfr1;

   MRS(mmfr1, id_aa64mmfr1_el1);
   return mmfr1;
}

static INLINE bool vhe_supported(void)
{
   uint64_t mmfr1 = get_mmfr1();

   if (!el_is_hyp()) {
      return false;
   }

   if ((mmfr1 & MMFR1_VH_MASK) == MMFR1_VH_NOT_PRESENT) {
      return false;
   }

   return true;
}

static INLINE uint64_t get_hcr(void)
{
   uint64_t hcr;

   MRS(hcr, hcr_el2);
   return hcr;
}

static INLINE bool vhe_enabled(void)
{
   uint64_t hcr;

   if (!el_is_hyp()) {
      return false;
   }

   hcr = get_hcr();
   if ((hcr & HCR_E2H) != 0) {
      return true;
   }

   return false;
}

static INLINE bool mmu_supported_configuration(void)
{
   int gran;
   uint64_t tcr;

   if (!is_paging_enabled()) {
      return false;
   }

   tcr = get_tcr();
   gran = (tcr >> TCR_ELx_TG0_SHIFT) & TCR_ELx_TG0_MASK;
   if (gran != TCR_GRANULARITY_4K) {
      /*
       * Not supposed to happen according to the UEFI
       * spec, but that has never stopped anyone before.
       */
      return false;
   }

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

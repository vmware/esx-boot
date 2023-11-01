/*******************************************************************************
 * Copyright (c) 2008-2011,2015-2016,2019-2020,2022-2023 VMware, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * cpu.h -- CPU-specific definitions
 */

#ifndef CPU_H_
#define CPU_H_

#include <compat.h>
#include <cpuid.h>
#include <sys/types.h>
#include <stdbool.h>

/*
 * PC compatibles have BIOS and option card ROMs here,
 * "low" RAM, partially used by BIOS, VGA RAM.
 */
#define LOW_IBM_PC_MEGABYTE 0x100000ULL

/*
 * Work around Intel erratum "Processor May Hang When Executing
 * Code In an HLE Transaction Region" CFL106, SKL170, KBL121,
 * SKW159, KBW114, SKZ63.  See PR 2140637.
 */
#define SKYLAKE_HLE_BLACKLIST_MA_LOW 0x40000000
#define SKYLAKE_HLE_BLACKLIST_MA_HIGH 0x40400000

#define CPUID_INTEL_VENDOR_STRING "GenuntelineI"

typedef struct CPUIDRegs {
   unsigned int eax, ebx, ecx, edx;
} CPUIDRegs;

extern CPUIDRegs cpuid0;
extern CPUIDRegs cpuid1;

#define CPUID_FAMILY_P6 6
#define CPUID_FAMILY_EXTENDED 15

#define CPUID_MODEL_SKYLAKE_4E 0x4e    // Skylake-Y / Kaby Lake U/Y ES macro
#define CPUID_MODEL_SKYLAKE_55 0x55    // Skylake EP/EN/EX macro
#define CPUID_MODEL_SKYLAKE_5E 0x5e    // Skylake-S / Kaby Lake S/H ES
#define CPUID_MODEL_CANNONLAKE_66 0x66 // Cannon Lake
#define CPUID_MODEL_KABYLAKE_8E 0x8e   // Kaby Lake U/Y QS
#define CPUID_MODEL_KABYLAKE_9E 0x9e   // Kaby Lake S/H QS

#define BIT_MASK(shift) (0xffffffffu >> (32 - shift))

/*
 * CPUID result registers
 */

#define CPUID_REGS                                                             \
   CPUIDREG(EAX, eax)                                                          \
   CPUIDREG(EBX, ebx)                                                          \
   CPUIDREG(ECX, ecx)                                                          \
   CPUIDREG(EDX, edx)

typedef enum {
#define CPUIDREG(uc, lc) CPUID_REG_##uc,
   CPUID_REGS
#undef CPUIDREG
      CPUID_NUM_REGS
} CpuidReg;

#define CPUID_LEVELS                                                           \
   CPUIDLEVEL(TRUE, 0, 0, 0, 0)                                                \
   CPUIDLEVEL(TRUE, 1, 1, 0, 0)                                                \
   CPUIDLEVEL(TRUE, 81F, 0x8000001F, 0, 14)

/* Define  CPUID levels in the form: CPUID_LEVEL_<ShortName> */
typedef enum {
#define CPUIDLEVEL(t, s, v, c, h) CPUID_LEVEL_##s,
   CPUID_LEVELS
#undef CPUIDLEVEL
      CPUID_NUM_LEVELS
} CpuidCachedLevel;

/* Enum to translate between shorthand name and actual CPUID level value. */
enum {
#define CPUIDLEVEL(t, s, v, c, h) CPUID_LEVEL_VAL_##s = (int) v,
   CPUID_LEVELS
#undef CPUIDLEVEL
};

#define FIELD(lvl, ecxIn, reg, bitpos, size, name, s, hwv)                     \
   CPUID_##name##_SHIFT = bitpos,                                              \
   CPUID_##name##_MASK = BIT_MASK(size) << bitpos,                             \
   CPUID_INTERNAL_SHIFT_##name = bitpos,                                       \
   CPUID_INTERNAL_MASK_##name = BIT_MASK(size) << bitpos,                      \
   CPUID_INTERNAL_REG_##name = CPUID_REG_##reg,                                \
   CPUID_INTERNAL_EAXIN_##name = CPUID_LEVEL_VAL_##lvl,                        \
   CPUID_INTERNAL_ECXIN_##name = ecxIn,

#define FLAG FIELD

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_1                                               \
   FIELD(1, 0, EAX, 0, 4, STEPPING, ANY, 4)                                    \
   FIELD(1, 0, EAX, 4, 4, MODEL, ANY, 4)                                       \
   FIELD(1, 0, EAX, 8, 4, FAMILY, YES, 4)                                      \
   FIELD(1, 0, EAX, 12, 2, TYPE, ANY, 4)                                       \
   FIELD(1, 0, EAX, 16, 4, EXTENDED_MODEL, ANY, 4)                             \
   FIELD(1, 0, EAX, 20, 8, EXTENDED_FAMILY, YES, 4)

/*    LEVEL, SUB-LEVEL, REG, POS, SIZE, NAME,               MON SUPP, HWV  */
#define CPUID_FIELD_DATA_LEVEL_81F                                             \
   FLAG(81F, 0, EAX, 1, 1, SEV, YES, 17)                                       \
   FIELD(81F, 0, EBX, 0, 6, SME_PAGE_TABLE_BIT_NUM, YES, 17 )

#define CPUID_FIELD_DATA                                                       \
   CPUID_FIELD_DATA_LEVEL_1                                                    \
   CPUID_FIELD_DATA_LEVEL_81F

enum {
   /* Define data for every CPUID field we have */
   CPUID_FIELD_DATA
};

#define CPUID_GET(eaxIn, reg, field, data)                                     \
   (((unsigned int)(data)&CPUID_INTERNAL_MASK_##field) >>                      \
    CPUID_INTERNAL_SHIFT_##field)

static INLINE bool __GET_CPUID(unsigned int leaf, CPUIDRegs *regs)
{
   return __get_cpuid(leaf, &regs->eax, &regs->ebx, &regs->ecx, &regs->edx);
}

static INLINE bool __GET_CPUID2(unsigned int leaf, unsigned int subleaf,
                                CPUIDRegs *regs)
{
   return __get_cpuid_count(leaf, subleaf, &regs->eax, &regs->ebx, &regs->ecx,
                            &regs->edx);
}

/* IN: %eax from CPUID with %eax=1. */
static INLINE int CPUID_EFFECTIVE_FAMILY(unsigned int v)
{
   unsigned int f = CPUID_GET(1, EAX, FAMILY, v);
   return f != CPUID_FAMILY_EXTENDED ? f : f +
      CPUID_GET(1, EAX, EXTENDED_FAMILY, v);
}

static INLINE bool CPUID_FAMILY_IS_P6(unsigned int eax)
{
   return CPUID_EFFECTIVE_FAMILY(eax) == CPUID_FAMILY_P6;
}

/* IN: %eax from CPUID with %eax=1. */
static INLINE unsigned int CPUID_EFFECTIVE_MODEL(unsigned int v)
{
   unsigned int m = CPUID_GET(1, EAX, MODEL, v);
   unsigned int em = CPUID_GET(1, EAX, EXTENDED_MODEL, v);
   return m + (em << 4);
}

/* IN: %eax from CPUID with %eax=1. */
static INLINE bool CPUID_UARCH_IS_SKYLAKE(unsigned int v)
{
   unsigned int model = 0;

   if (!CPUID_FAMILY_IS_P6(v)) {
      return false;
   }

   model = CPUID_EFFECTIVE_MODEL(v);

   return (model == CPUID_MODEL_SKYLAKE_4E    ||
           model == CPUID_MODEL_SKYLAKE_55    ||
           model == CPUID_MODEL_SKYLAKE_5E    ||
           model == CPUID_MODEL_CANNONLAKE_66 ||
           model == CPUID_MODEL_KABYLAKE_8E   ||
           model == CPUID_MODEL_KABYLAKE_9E);
}

static INLINE bool CPUID_IsRawVendor(CPUIDRegs *id0, const char* vendor)
{
   return (id0->ebx == *(const unsigned int *)(uintptr_t) (vendor + 0) &&
           id0->ecx == *(const unsigned int *)(uintptr_t) (vendor + 4) &&
           id0->edx == *(const unsigned int *)(uintptr_t) (vendor + 8));
}

static INLINE bool CPUID_IsVendorIntel(CPUIDRegs *id0)
{
   return CPUID_IsRawVendor(id0, CPUID_INTEL_VENDOR_STRING);
}

/*
 * Intel TDX
 */
#define CPUID_INTEL_TDX_VENDOR_STRING "Inte    lTDX"

#define TDX_TDCALL_OPCODE ".byte 0x66, 0x0F, 0x01, 0xCC"
#define TDX_TDCALL_TDG_VP_INFO 1
#define TDX_GPAW_MASK 0x3F

#define TDX_STATUS_SUCCESS 0

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

#define CR4_ATTR_LA57 (1 << 12)

static INLINE void get_cr4(uintptr_t *cr4)
{
   __asm__ __volatile__("mov %%cr4, %0"
                        : "=r" (*cr4));
}

/*
 * Paging
 */
#define PAGE_SIZE  0x1000ULL
#define PG_TABLE_MAX_ENTRIES 512
#define PG_TABLE_MAX_LEVELS 4

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

#define PG_IS_LARGE(level, entry) ((entry & PG_ATTR_PAGE_SIZE) != 0)
#define PG_IS_READONLY(entry) ((entry & PG_ATTR_W) == 0)
#define PG_ENTRY_TO_PG(entry) ((uint64_t *) (entry & PG_FRAME_MASK))
#define PG_CLEAN_READONLY(entry) ((entry) | PG_ATTR_W)
#define PG_CLEAN_TABLE_READONLY(entry) (entry)
#define PG_CLEAN_NOEXEC(entry) ((entry) & ~PG_ATTR_XD)
#define PG_CLEAN_TABLE_NOEXEC(entry) (entry)
/*
 * x86 has no concept of hierarchical attributes in entries pointing
 * to page tables.
 */
#define PG_TABLE_XD_RO_2_PAGE_ATTRS(entry) 0

static inline uint64_t PG_ENTRY_TO_PAGE_FLAGS(UNUSED_PARAM(unsigned level),
                                              uint64_t entry)
{
   uint64_t flags;

   if (PG_IS_LARGE(level, entry)) {
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
   CPUIDRegs regs;

   /*
    * If TDX is suported we must mask off the SHARED bit when mapping the PTEs.
    */
   if (__GET_CPUID2(0x21, 0, &regs) &&
       CPUID_IsRawVendor(&regs, CPUID_INTEL_TDX_VENDOR_STRING)) {
      uint64_t status, gpaw;
      __asm__ __volatile__(TDX_TDCALL_OPCODE
                           : "=a" (status), "=c" (gpaw)
                           : "a" (TDX_TDCALL_TDG_VP_INFO)
                           : "rdx", "r8", "r9", "r10", "r11");
      if (status == TDX_STATUS_SUCCESS) {
         uint8_t shared_bit = (gpaw & TDX_GPAW_MASK) - 1;
         return (1ull << shared_bit) | PG_ATTR_MASK;
      }
   }

   /*
    * If SEV is supported we must mask off the memory encryption bit when
    * mapping the PTEs.
    */
   if (__GET_CPUID(0x8000001F, &regs) &&
       CPUID_GET(0x8000001F, EAX, SEV, regs.eax)) {
      uint8_t c_bit = CPUID_GET(0x8000001F, EBX, SME_PAGE_TABLE_BIT_NUM,
                                regs.ebx);
      return (1ull << c_bit) | PG_ATTR_MASK;
   }

   return PG_ATTR_MASK;
}

static INLINE bool is_paging_enabled(void)
{
   uintptr_t cr0;

   get_cr0(&cr0);
   return ((cr0 & CR0_ATTR_PG) != 0);
}

/*-- pg_table_levels -----------------------------------------------------------
 *
 *      Return the number of page table levels.  Assumes paging is
 *      enabled and the CPU is in 64-bit mode.
 *----------------------------------------------------------------------------*/
static INLINE int pg_table_levels(void)
{
   uintptr_t cr4;

   get_cr4(&cr4);
   return (cr4 & CR4_ATTR_LA57) == 0 ? 4 : 5;
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

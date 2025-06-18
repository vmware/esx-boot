/*******************************************************************************
 * Copyright (c) 2022 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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

#define CSR_SSTATUS 0x100
#define SSTATUS_SIE (1UL << 1)

/*
 * CSR accessors.
 */
#define CSR_READ(reg, csr) {                                \
      __asm__ __volatile__ ("csrr %0, " _STRINGIFY(csr)     \
                            : "=r" (reg) :                  \
                            : "memory");                    \
   }

#define CSR_WRITE(csr, val) {                                  \
      __asm__ __volatile__ ("csrw " _STRINGIFY(csr) ", %0"     \
                            : : "r" ((uint64_t) val)           \
                            : "memory");                       \
   }

/*
 * Barriers.
 */
#define fence_io_read() __asm__ __volatile__("fence i,ir" : : : "memory")
#define fence_io_write() __asm__ __volatile__("fence ow,o" : : : "memory")

/*
 * Interrupts.
 */
static INLINE void CLI(void)
{
   uint64_t reg;

   CSR_READ(reg, CSR_SSTATUS);
   reg &= ~SSTATUS_SIE;
   CSR_WRITE(CSR_SSTATUS, reg);
}

static INLINE void STI(void)
{
   uint64_t reg;

   CSR_READ(reg, CSR_SSTATUS);
   reg |= SSTATUS_SIE;
   CSR_WRITE(CSR_SSTATUS, reg);
}

static INLINE void HLT(void)
{
   while (1) {
      __asm__ __volatile__ ("wfi");
   }
}

/*
 * Paging
 */
#define PAGE_SIZE ((uint64_t)0x1000)

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
static ALWAYS_INLINE void cpu_code_update(UNUSED_PARAM(uintptr_t va), UNUSED_PARAM(uint64_t len))
{
   /* Nothing to do here, apparently. */
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
   __asm__ __volatile__ ("fence.i" ::: "memory");
}

#endif /* !CPU_H_ */

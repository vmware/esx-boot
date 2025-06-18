/*******************************************************************************
 * Copyright (c) 2008-2023 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * compat.h -- Portability across building environments
 */

#ifndef COMPAT_H_
#define COMPAT_H_

#include <compiler.h>
#include <error.h>
#include <sys/types.h>

#if defined(only_riscv64)
#define arch_is_riscv64 1
#define arch_name "riscv64"
#else
#define arch_is_riscv64 0
#endif

#if defined(only_arm64)
#define arch_is_arm64 1
#define arch_name "arm64"
#else
#define arch_is_arm64 0
#endif

#if defined(only_arm64) || defined(only_em64t) || defined(only_riscv64)
#define arch_is_64 1
#else
#define arch_is_64 0
#endif

#if defined(only_x86)
/*
 * only_x86 is set for either 32- or 64-bit x86.
 */
#define arch_is_x86 1
#if arch_is_64
#define arch_name "x86"
#else
#define arch_name "x86_32"
#endif
#else
#define arch_is_x86 0
#endif

/*
 * Debugging
 */
#ifdef DEBUG
#   define ASSERT_GENERIC(_expr_, _callback_, _msg_)              \
       do {                                                       \
          if (!(_expr_)) {                                        \
             (_callback_)(__FILE__ " (%d): "                      \
                          "assert (" _msg_ ")\n", __LINE__);      \
             for ( ; ; );                                         \
          }                                                       \
       } while (0)
#else
#   define ASSERT_GENERIC(_expr_, _callback_, _msg_)
#endif

/*
 * Various usual macro
 */
#define IS_ALIGNED(_addr_, _alignment_)                           \
   (((_addr_) % (_alignment_)) == 0)

#define STRSIZE(_str_)                                            \
   (strlen(_str_) + 1)

#ifndef ARRAYSIZE
#   define ARRAYSIZE(_array_) (sizeof (_array_) / sizeof ((_array_)[0]))
#endif

#if !defined(MIN)
#   define MIN(a,b)  (((a)<(b))?(a):(b))
#endif
#if !defined(MAX)
#   define MAX(a,b)  (((a)>(b))?(a):(b))
#endif

#define _STRINGIFY(arg) #arg
#define STRINGIFY(arg) _STRINGIFY(arg)

#define ROUNDDOWN(x,y)((x) / (y) * (y))

/*
 * The main purpose of the uint64_generic_t type and the derived inline
 * functions is to bring up portability across 32-bit compilers that do not
 * support shifting, multiplying and dividing 64-bit integers.
 *
 * On these compilers, isolating the 4 MSB of a 64-bit value is not possible
 * using the trivial expression (ui64 >> 32).
 */
#pragma pack(1)
typedef union {
   struct {
      uint32_t low;
      uint32_t high;
   } ui32;

   uint64_t ui64;
} uint64_generic_t;
#pragma pack()

static INLINE uint64_t uint32_concat(uint32_t high, uint32_t low)
{
   uint64_generic_t llu;

   llu.ui32.low = low;
   llu.ui32.high = high;

   return llu.ui64;
}

static INLINE uint32_t highhalf64(uint64_t u)
{
   uint64_generic_t llu;

   llu.ui64 = u;

   return llu.ui32.high;
}

static INLINE uint32_t lowhalf64(uint64_t u)
{
   uint64_generic_t llu;

   llu.ui64 = u;

   return llu.ui32.low;
}

#endif /* COMPAT_H_ */

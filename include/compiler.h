/*******************************************************************************
 * Copyright (c) 2008-2022 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * compiler.h -- Compiler-dependent definitions
 */

#ifndef COMPILER_H_
#define COMPILER_H_

/*
 * Force symbols visibility to hidden (assume that all symbols resolve within
 * the final binary). This avoids generating GOT/PLT relocations and makes the
 * ELF to EFI binary conversion much easier.
 */
#ifndef __COM32__
#  if __GNUC__ >= 4
#     pragma GCC visibility push(hidden)
#  else
#     error "gcc v4.0 or later required."
#  endif
#endif

#define EXTERN extern
#define INLINE __inline__
#define ALWAYS_INLINE __attribute__ ((always_inline)) INLINE

#if defined(only_em64t) || defined(only_arm64) || defined(only_riscv64)
#  define CDECL
#else
#  define CDECL  __attribute__ ((cdecl,regparm(0)))
#endif

#define SECTION(_section_) __attribute__ ((section(_section_)))

#define UNUSED_PARAM(_param_) _param_ __attribute__ ((__unused__))

#endif /* !COMPILER_H_ */

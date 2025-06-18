/*******************************************************************************
 * Copyright (c) 2008-2022 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * stddef.h
 */

#ifndef _STDDEF_H
#define _STDDEF_H

#include <compiler.h>

#define _SIZE_T
#if defined(_MSC_VER)
#   if defined(only_em64t) || defined(only_arm64) || defined(only_riscv64)
       typedef unsigned __int64        size_t;
#   else
       typedef unsigned int            size_t;
#   endif
#elif defined(__GNUC__)
#   if defined(only_em64t) || defined(only_arm64) || defined(only_riscv64)
       typedef unsigned long int       size_t;
#   else
       typedef unsigned int            size_t;
#   endif
#else
#   error "Need compiler define"
#endif

typedef size_t off_t;

#define _PTRDIFF_T
#if defined(_MSC_VER)
#   if defined(only_em64t) || defined(only_arm64) || defined(only_riscv64)
       typedef __int64                 ptrdiff_t;
#   else
       typedef signed int              ptrdiff_t;
#   endif
#elif defined (__GNUC__)
#   if defined(only_em64t) || defined(only_arm64) || defined(only_riscv64)
       typedef signed long int         ptrdiff_t;
#   else
       typedef signed int              ptrdiff_t;
#   endif
#else
#   error "Need compiler define"
#endif

#undef NULL
#define NULL   ((void *)0)

#undef offsetof
#define offsetof(_type_, _field_)   ((size_t)&((_type_ *)0)->_field_)

#endif /* !_STDDEF_H */

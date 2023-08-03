/*******************************************************************************
 * Copyright (c) 2022 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

#ifndef __BPATCH_MATCH_GRP_ARCH_H__
#define __BPATCH_MATCH_GRP_ARCH_H__

/*
 * Create an Id from the SYstem Register
 */
#define MASK(n)              ((1 << (n)) - 1)
#define BP_MRS_MSR_SYSREG(op0, op1, n, m, op2) \
              (((op0) & MASK(1)) << 19 | (op1) << 16 | (n) << 12 | (m) << 8 \
               | (op2) << 5)
#define _SYSREG(name, op0, op1, crn, crm, op2)                      \
 BP_SYSREG_##name =  BP_MRS_MSR_SYSREG(op0, op1, crn, crm, op2),

typedef enum {
#include "arm64_sysreg_table.h"
} sysRegId_t;
#undef _SYSREG
#endif

/*******************************************************************************
 * Copyright (c) 2021 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * fdt_vmware.h -- additional VMware-specific wrappers for libfdt.
 */

#ifndef FDT_VMWARE_H_
#define FDT_VMWARE_H_

#include <libfdt.h>

typedef uint32_t fdt_cell_t;

int fdt_get_reg(void *fdt, int node, const char *prop, uintptr_t *base);

#endif /* FDT_VMWARE_H_ */

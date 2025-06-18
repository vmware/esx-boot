/*******************************************************************************
 * Copyright (c) 2021-2022 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * fdt_vmware.h -- additional VMware-specific wrappers for libfdt.
 */

#ifndef FDT_VMWARE_H_
#define FDT_VMWARE_H_

#include <libfdt.h>
#include <boot_services.h>

typedef uint32_t fdt_cell_t;

typedef struct  {
   const char *id;
   serial_type_t type;
} fdt_serial_id_t;

int fdt_get_reg(void *fdt, int node, const char *prop, uintptr_t *base);
int fdt_match_system(void *fdt, const char *match);
int fdt_match_serial_port(void *fdt, const char *path, const char *prop_name,
                          const fdt_serial_id_t *match_ids, int *node_out,
                          serial_type_t *type, const char **baud_string);

#endif /* FDT_VMWARE_H_ */

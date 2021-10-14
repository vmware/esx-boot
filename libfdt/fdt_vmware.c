/*******************************************************************************
 * Copyright (c) 2021 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * fdt_vmware.c -- additional VMware-specific wrappers for libfdt.
 */

#include <fdt_vmware.h>

/*-- fdt_get_reg ---------------------------------------------------------------
 *
 *      Parses a "reg" property.
 *
 * Parameters
 *      IN fdt: the FDT.
 *      IN node: node offset.
 *      IN prop: property.
 *      OUT base: pointer to store base.
 *
 * Results
 *      Length of reg, 0 if unknown, or a libfdt error code.
 *----------------------------------------------------------------------------*/

int fdt_get_reg(void *fdt, int node, const char *prop, uintptr_t *base)
{
   int parent;
   int addr_cells;
   int size_cells;
   const fdt_cell_t *data;

   data = fdt_getprop(fdt, node, prop, NULL);
   if (data == NULL) {
      return -FDT_ERR_NOTFOUND;
   }

   parent = fdt_parent_offset(fdt, node);
   if (fdt_getprop(fdt, node, "#address-cells", NULL) || parent < 0) {
      addr_cells = fdt_address_cells(fdt, node);
   } else {
      addr_cells = fdt_address_cells(fdt, parent);
   }

   if (addr_cells == 1) {
      *base = fdt32_to_cpu(*data);
   } else {
      *base = fdt64_to_cpu(*(uint64_t *) data);
   }

   if (fdt_getprop(fdt, node, "#size-cells", NULL) || parent < 0) {
      size_cells = fdt_size_cells(fdt, node);
   } else {
      size_cells = fdt_size_cells(fdt, parent);
   }

   data += addr_cells;

   if (size_cells == 1) {
      return fdt32_to_cpu(*data);
   } else {
      return fdt64_to_cpu(*(uint64_t *) data);
   }

   return 0;
}

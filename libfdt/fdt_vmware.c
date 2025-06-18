/*******************************************************************************
 * Copyright (c) 2021-2022 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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


/*-- fdt_match_system ----------------------------------------------------------
 *
 *      Returns ERR_SUCCESS if the compatible or model properties match
 *      the passed parameter.
 *
 * Parameters
 *      IN fdt: the FDT.
 *      IN match: string to match compatible/model properties.
 *
 * Results
 *      0 or -FDT_ERR_NOTFOUND.
 *----------------------------------------------------------------------------*/

int fdt_match_system(void *fdt, const char *match)
{
   int node;

   node = fdt_path_offset(fdt, "/");
   if (node < 0) {
      return -FDT_ERR_NOTFOUND;
   }

   if (!fdt_node_check_compatible(fdt, node, match) ||
       !strcmp(match, (char *) fdt_getprop(fdt, node, "model", NULL))) {
      return 0;
   }

   return -FDT_ERR_NOTFOUND;
}


/*-- fdt_match_serial_port -----------------------------------------------------
 *
 *      Given a node and a property encoding a different device tree node,
 *      compare the the 2nd node's 'compatible' against a list of matching IDs,
 *      filling out *node_out, *type and *baud_out on success.
 *
 *      Simplifies parsing /chosen/stdout-path, /aliases/uart0 and so on.
 *
 * Parameters
 *      IN  fdt:  fdt blob
 *      IN  path: path to fdt node
 *      IN  prop_name: property name
 *      IN  match_ids: array of IDs to match
 *      OUT node_out: matched node
 *      OUT type: matched type
 *      OUT baud_out: matched baud string
 *
 * Results
 *      0 or -FDT_ERR_NOTFOUND.
 *----------------------------------------------------------------------------*/
int fdt_match_serial_port(void *fdt, const char *path, const char *prop_name,
                          const fdt_serial_id_t *match_ids, int *node_out,
                          serial_type_t *type, const char **baud_out)
{
   int node;
   int prop_len;
   const char *baud;
   const char *prop_value;
   const fdt_serial_id_t *idp;

   node = fdt_path_offset(fdt, path);
   if (node < 0) {
      return -FDT_ERR_NOTFOUND;
   }

   prop_value = fdt_getprop(fdt, node, prop_name, &prop_len);
   if (prop_value == NULL || prop_len == 0) {
      return -FDT_ERR_NOTFOUND;
   }

   /*
    * stdout-path will look like "serial0:1500000", where the thing
    * after the : takes the form of <baud>{<parity>{<bits>{<flow>}}}.
    *
    * baud - baud rate in decimal.
    * parity - 'n' (none), 'o', (odd) or 'e' (even).
    * bits - number of data bits.
    * flow - 'r' (rts).
    *
    * For example: serial0:1500000n8r. We don't attempt to parse
    * anything beyond the baud rate.
    *
    * It could also look like "simple-framebuffer", i.e. not be a serial port
    * at all...
    */
   baud = memchr(prop_value, ':',  prop_len);
   if (baud != NULL) {
      prop_len = baud - prop_value;
      baud++;
   } else {
      /*
       * Ignore the NUL.
       */
      prop_len--;
   }

   node = fdt_path_offset_namelen(fdt, prop_value, prop_len);
   if (node < 0) {
      return -FDT_ERR_NOTFOUND;
   }

   for (idp = match_ids; idp->id != NULL; idp++) {
      if (!fdt_node_check_compatible(fdt, node, idp->id)) {
         *type = idp->type;
         *baud_out = baud;
         *node_out = node;
         return 0;
      }
   }

   return -FDT_ERR_NOTFOUND;
}

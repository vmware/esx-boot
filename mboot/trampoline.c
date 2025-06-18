/*******************************************************************************
 * Copyright (c) 2008-2020 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

#include "mboot.h"

static ALWAYS_INLINE bool reloc_object_might_be_executable(reloc_t *obj)
{
   if (obj->type == 'k' || obj->type == 't') {
      return true;
   }

   return false;
}

/*-- do_reloc ------------------------------------------------------------------
 *
 *      Process the relocations by moving all the objects in the relocation
 *      table from their boot-time source to their run-time destination.
 *      This function assumes that the relocation table is NULL-terminated.
 *
 *   WARNING:
 *      In the case a run-time object had to be relocated where do_reloc() keeps
 *      its own code and/or data, this function would overwrite itself.
 *      For that reason, do_reloc() must be relocated into safe memory, and to
 *      do so, MUST BE POSITION-INDEPENDENT.
 *
 * Parameters
 *      IN reloc: the table of relocations
 *----------------------------------------------------------------------------*/
void TRAMPOLINE do_reloc(reloc_t *reloc)
{
   char *src, *dest;
   size_t size;

   for ( ; reloc->type != 0; reloc++) {
      src = reloc->src;

      dest = UINT64_TO_PTR(reloc->dest);
      size = (size_t)reloc->size;

      if (src == NULL) {
         /* bzero */
         while (size > 0) {
            dest[--size] = 0;
         }
      } else if (src != dest) {
         /* memmove */
         if (src < dest) {
            while (size > 0) {
               size--;
               dest[size] = src[size];
            }
         } else {
            while (size > 0) {
               *dest++ = *src++;
               size--;
            }
         }
      }

      if (reloc_object_might_be_executable(reloc)) {
         cpu_code_update(reloc->dest, reloc->size);
      }
   }

   cpu_code_update_commit();
}

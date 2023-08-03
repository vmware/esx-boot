/*******************************************************************************
 * Copyright (c) 2022 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * bpatch_elem.h --
 *
 *      Binary Patching element description. This structure is used for storing
 *      in dedicated ELF section information that will be used by system when
 *      they are applied.
 */

#ifndef __BPATCH_ELEM_H__
#define __BPATCH_ELEM_H__

#include <stdint.h>

typedef enum {
   FUNCTION_PATCH = 1,
   ZONE_PATCH     = 2,
   DATA_PATCH     = 3
} PatchType;

/*
 * This structure is used to store information for each patch. It is stored
 * into an ELF section. ELF element have to be 64bits
 */
#pragma pack(push)  // push current alignment to stack
#pragma pack(1)     // set alignment to 1 byte boundary
typedef struct {
   PatchType type;
   bool isApplied;
   char pad_0[3];
   void *functionToPatchAddr;
   void *patchedFunctionAddr;
   void *patchLocationAddr;
   union {
      struct {
         uint32_t pad_1;
         uint32_t newOpcode;
      };
      uint64_t newValue;
   };
   uint32_t writeSize;
   uint32_t patchGroupId;
} BinaryPatch;
#pragma pack(pop)   // restore original alignment from stack

#endif

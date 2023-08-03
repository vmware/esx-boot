/*******************************************************************************
 * Copyright (c) 2022 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * bpatch_arch.c --
 *
 *      BPATCH main methods for implementing binary patching platform specific
 *      code.
 */


#include <stdint.h>
#include <error.h>
#include <bootlib.h>
#include <compat.h>
#include "../bpatch.h"

/*
 * Variables used for implementation
 */
static const uint32_t branchOpcode = 0x14000000; // "B" instruction
extern uint64_t bp_offset;

/*
 * Defines used for implementation
 */
#define FUNC_PREAMBLE_SIZE  (4 * sizeof (uint32_t))
#define SIZE_OF_AARCH64_INSTRUCTION (sizeof (uint32_t))


extern int bpatch_find_placeholder(void *srcAddr, void **patchAddr);

/*-- bpatch_reg_system_detect ------------------------------------------
 *
 *     This method uses ARM register to decide or not to select the
 *     patch class. An enum is provided as parameter which is used for
 *     selecting the ARM register MIDR, PFR0, EL. The Matching Patch
 *     Group is also used as informations and the values obtained on the
 *     current platform are compared with the ones expected for applying
 *     the patch class.
 *
 * Parameters:
 *      IN matchPatchGroupElem: pointer to matching patch description
 *      IN matchType: type of matching.
 *--------------------------------------------------------------------*/
int bpatch_reg_system_detect(BPMatchPatchDesc_t *matchPatchGroupElem,
                             match_type_t matchType)
{
   int status = ERR_NOT_FOUND;

   if (matchType.type != MATCH_ARM_SYS_REG) {
      return ERR_NOT_FOUND;
   }

   if (matchPatchGroupElem != NULL) {
      uint64_t regMask  = matchPatchGroupElem->registerProcess.regMask;
      uint64_t regValue = matchPatchGroupElem->registerProcess.regValue;
      uint64_t readValue;
      sysRegId_t type = matchType.mrsValue;

      if (type == BP_SYSREG_CurrentEL) {
         __asm__("mrs %0, CurrentEL \n\t":"=r"(readValue):);
         status = ERR_SUCCESS;
      } else if (type == BP_SYSREG_MIDR_EL1) {
         uint32_t regVal;
         __asm__("mrs %0, MIDR_EL1 \n\t":"=r"(regVal):);
         readValue = regVal;
         status = ERR_SUCCESS;
      } else if (type == BP_SYSREG_ID_AA64PFR0_EL1) {
         __asm__("mrs %0, ID_AA64PFR0_EL1 \n\t":"=r"(readValue):);
         status = ERR_SUCCESS;
      }

      if ((status == ERR_SUCCESS && (readValue & regMask) == regValue)) {
         status = ERR_SUCCESS;
      } else {
         status = ERR_NOT_FOUND;
      }
   }
   return status;
}

/*-- bpatch_modify_opcode ----------------------------------------------
 *
 *      This function modifies the opcode at the given address.
 *
 * Parameters:
 *      IN patchAddr: address of memory to patch structure.
 *      IN opcode: the new opcode to insert
 *--------------------------------------------------------------------*/
void bpatch_modify_opcode(void *patchAddr, uint32_t opcode)
{
   uint32_t *ptr = (uint32_t *)((uint64_t)patchAddr + bp_offset);

   Log(LOG_DEBUG, "- The opcode 0x%x is replaced with 0x%x",
       *((uint32_t *)ptr),
       opcode);

   /* modify the code inserting a branch to pc offset. */
   *((uint32_t *)ptr) = opcode;
}

/*-- bpatch_apply_func_patch -------------------------------------------
 *
 *      Replace the NOP created by the compiler with provided the opcode
 *      in order to branch upto the new function.
 *
 * Parameters:
 *      IN patch Element: point to patch element structure
 *--------------------------------------------------------------------*/
int bpatch_apply_func_patch(BinaryPatch *patchElement)
{
   int status;
   void *patchAddr;
   void *srcAddr = patchElement->functionToPatchAddr;
   void *dstAddr = patchElement->patchedFunctionAddr;

   if (patchElement->writeSize != SIZE_OF_AARCH64_INSTRUCTION) {
      Log(LOG_ERR, "Expected instruction size is %lx but we have %x",
          SIZE_OF_AARCH64_INSTRUCTION, patchElement->writeSize);
      return ERR_INVALID_PARAMETER;
   }

   Log(LOG_DEBUG, "Let's patch a Function at  %p with %p", srcAddr, dstAddr);
   status = bpatch_find_placeholder(srcAddr, &patchAddr);
   if (status == ERR_SUCCESS) {
      int offset26bits;
      uint32_t opcode;

      /*
       * Calculate the offset and the opcode.
       * 0x14000000 + Imm26
       */
      offset26bits = (intptr_t)dstAddr - (intptr_t)patchAddr;
      offset26bits = offset26bits >> 2;
      opcode = branchOpcode | (offset26bits & MASK(26));

      /* Let's modify the opcode. */
      bpatch_modify_opcode(patchAddr, opcode);
      patchElement->isApplied = TRUE;
      patchElement->patchLocationAddr = patchAddr;
   }

   return status;
}

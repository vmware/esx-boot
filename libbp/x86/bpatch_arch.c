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

/*-- bpatch_reg_system_detect ------------------------------------------
 *
 *     This method uses CPU register to decide or not to select the
 *     patch class. An enum is provided as parameter which is used for
 *     selecting the CPU register. The Matching Patch Group is also used
 *     for informations and the values obtained on the current platform
 *     are compared with the ones expected for applying the patch class.
 *
 * Parameters:
 *      IN matchPatchGroupElem: pointer to matching patch structure.
 *      IN matchType: matching type used.
 *--------------------------------------------------------------------*/
int bpatch_reg_system_detect(UNUSED_PARAM(BPMatchPatchDesc_t
                                          *matchPatchGroupElem),
                             UNUSED_PARAM(match_type_t matchType))
{
   return ERR_NOT_FOUND;
}

/*-- bpatch_modify_opcode ----------------------------------------------
 *
 *      This function modifies the opcode at the given address.
 *
 * Parameters:
 *      IN patchAddr: memory address pointer to patch.
 *      IN opcode: the opcode to insert.
 *--------------------------------------------------------------------*/
void bpatch_modify_opcode(UNUSED_PARAM(void *patchAddr),
                          UNUSED_PARAM(uint32_t opcode))
{
}

/*-- bpatch_apply_func_patch -------------------------------------------
 *
 *      Replace the NOP created by the compiler by the opcode to
 *      branch upto the patched function.
 *
 * Parameters:
 *      IN patchElement: pointer to patch structure.
 *--------------------------------------------------------------------*/
int bpatch_apply_func_patch(UNUSED_PARAM(BinaryPatch *patchElement))
{
   return ERR_NOT_FOUND;
}

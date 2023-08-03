/*******************************************************************************
 * Copyright (c) 2022 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * bpatch.h --
 *
 *      Binary Patching interface. Those methods are used for applying some
 *      binary patches to an ELF software.
 */

#ifndef __BPATCH_H__
#define __BPATCH_H__

#include "bpatch_elem.h"
#include "bpatch_match_group.h"

#define TRUE  (1 == 1)
#define FALSE (0 == 1)

extern int bpatch_apply_patch(BinaryPatch *patchElement, uint32_t patchGroupId);
extern int bpatch_get_patch_grpid(BPMatchPatchDesc_t *patchDescElement,
                                  patch_group_t *patchGroupId);
extern void bpatch_set_patchloc(void *sectionAddr, uint32_t sectionSize);
extern void bpatch_set_offset(uint64_t off);
#endif

/*******************************************************************************
 * Copyright (c) 2022 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * bapply.h --
 *
 *      Binary Patching related functionality.
 */

#ifndef BP_APPLY_H
#define BP_APPLY_H

#include <stdint.h>

int bapply_patch_esxinfo(void* ehdr);

#endif

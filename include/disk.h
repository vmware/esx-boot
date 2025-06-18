/*******************************************************************************
 * Copyright (c) 2008-2011 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * disk.h -- Disk abstraction shared by both low-level and high-level APIs
 */

#ifndef DISK_H_
#define DISK_H_

typedef struct disk_t {
   uintptr_t firmware_id;
   bool use_edd;
   uint32_t cylinders;
   uint32_t heads_per_cylinder;
   uint32_t sectors_per_track;
   uint16_t bytes_per_sector;
} disk_t;

#endif

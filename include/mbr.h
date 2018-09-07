/*******************************************************************************
 * Copyright (c) 2008-2011 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * mbr.h -- Master Boot Record partition table data structures
 */

#ifndef MBR_H_
#define MBR_H_

#include <stdint.h>

#define PART_TYPE_EMPTY                 0x0
#define PART_TYPE_FAT12                 0x1
#define PART_TYPE_EXTENDED              0x5
#define PART_TYPE_FAT16                 0x6
#define PART_TYPE_WIN_EXTENDED          0xf
#define PART_TYPE_LINUX_EXTENDED        0x85
#define PART_TYPE_GPT                   0xee

#pragma pack(1)
typedef struct {
   uint8_t flags;
   uint8_t start_head;
   uint16_t start_cylsec;
   uint8_t type;
   uint8_t end_head;
   uint16_t end_cylsec;
   uint32_t start_lba;
   uint32_t sectors_num;
} mbr_part_t;
#pragma pack()

#define PRIMARY_PARTITION_ENTRY(mbr, entrynum)     \
   &((mbr_part_t *)(mbr + 0x1be))[entrynum - 1];

#define PART_IS_PROTECTIVE_MBR(_part_)                        \
   ((_part_)->type == PART_TYPE_GPT &&                        \
    (_part_)->start_lba == 1 &&                               \
    (_part_)->flags == 0)

#endif

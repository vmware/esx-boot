/*******************************************************************************
 * Copyright (c) 2008-2011,2021 VMware, Inc.  All rights reserved.
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
#define PART_TYPE_FAT16_LT32MB          0x4
#define PART_TYPE_EXTENDED              0x5
#define PART_TYPE_FAT16                 0x6
#define PART_TYPE_FAT32                 0xb
#define PART_TYPE_FAT32_LBA             0xc
#define PART_TYPE_FAT16_LBA             0xe
#define PART_TYPE_WIN_EXTENDED          0xf
#define PART_TYPE_LINUX_EXTENDED        0x85
#define PART_TYPE_NON_FS                0xda
#define PART_TYPE_GPT_PROTECTIVE        0xee
#define PART_TYPE_EFI                   0xef

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
   ((_part_)->type == PART_TYPE_GPT_PROTECTIVE &&             \
    (_part_)->start_lba == 1 &&                               \
    (_part_)->flags == 0)

#endif

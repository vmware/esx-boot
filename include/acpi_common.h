/*******************************************************************************
 * Copyright (c) 2019-2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * acpi_common.h -- ACPI structures definitions common to all architectures
 */

#ifndef ACPI_COMMON_H_
#define ACPI_COMMON_H_

#include <sys/types.h>

#pragma pack(1)
typedef struct {
   uint64_t signature;
   uint8_t checksum;
   uint8_t oem_id[6];
#define ACPI_RSDP_V2 2
   uint8_t revision;
   uint32_t rsdt_address;
   uint32_t length;
   /*
    * The following fields are only valid,
    * when revision is >= 2.
    */
   uint64_t xsdt_address;
   uint8_t extended_checksum;
   uint8_t reserved[3];
} acpi_rsdp;
#pragma pack()

#pragma pack(1)
typedef struct {
   uint32_t signature;
   uint32_t length;
   uint8_t revision;
   uint8_t checksum;
   uint8_t oem_id[6];
   uint8_t table_id[8];
   uint32_t oem_revision;
   uint32_t creator_id;
   uint32_t creator_revision;
} acpi_sdt;
#pragma pack()

#endif /* !ACPI_COMMON_H_ */

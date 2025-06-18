/*******************************************************************************
 * Copyright (c) 2019-2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc.
 * and/or its subsidiaries.
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

/*
 * CXL related definitions
 */
#define ACPI_CEDT_STRUCT_TYPE_CHBS 0x0
#define ACPI_CEDT_STRUCT_TYPE_CFMWS 0x1

#pragma pack(push, 1)
typedef struct acpi_cedt_struct_header {
   uint8_t type;       // 00h: CHBS, 01h: CFMWS
   uint8_t _reserved;
   uint16_t length;   // length of this structure
} acpi_cedt_struct_header;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct acpi_cedt_chbs_struct {
   acpi_cedt_struct_header header;   // ACPI header
   uint32_t uid;              // associated host bridge unique ID
   uint32_t _dont_care[6];
} acpi_cedt_chbs_struct;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct acpi_cedt_cfmws_struct {
   acpi_cedt_struct_header header;  // ACPI header
   uint32_t _reserved1;             // reserved
   uint64_t baseHPA;                // base host physical address of the window
   uint64_t windowSize;
   uint8_t interleaveWays;
   uint8_t  interleave_arithematic;  // method used for HPA mapping
   uint16_t reserved2;              // reserved
   uint32_t granularity;            // interleave granularity
   uint16_t restrictions;           // HPA use restrictions
   uint16_t qtgID;                  // QoS Throttling Group ID
   uint32_t targetList[];             // target list, should match with CHBS IDs
} acpi_cedt_cfmws_struct;
#pragma pack(pop)

#pragma pack(1)
typedef struct acpi_cedt_table {
   acpi_sdt header;
   uint8_t structs[];
} acpi_cedt_table;
#pragma pack()

#endif /* !ACPI_COMMON_H_ */

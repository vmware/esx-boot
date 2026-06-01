/*******************************************************************************
 * Copyright (c) 2008-2018 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * sm_bios.h -- SMBIOS structures definitions
 */

#ifndef SM_BIOS_H_
#define SM_BIOS_H_

#include <sys/types.h>

#define SMBIOS_PARAGRAPH_SIZE      16

#define SMBIOS_EPS_SIGNATURE       "_SM_"
#define SMBIOS_EPS_SIGNATURE_LEN   4

#pragma pack(1)
typedef struct {
   char anchor_string[SMBIOS_EPS_SIGNATURE_LEN]; /* '_SM_' */
   uint8_t checksum;
   uint8_t length;
   uint8_t major_version;
   uint8_t minor_version;
   uint16_t max_struct_size;
   uint8_t entry_point_revision;
   uint8_t formatted_area[5];
   char intermediate_anchor_string[5];    /* '_DMI_' */
   uint8_t intermediate_checksum;
   uint16_t table_length;
   uint32_t table_address;
   uint16_t struct_number;
   uint8_t version_bcd;
} smbios_eps;
#pragma pack()

#define SMBIOS_EPS3_SIGNATURE       "_SM3_"
#define SMBIOS_EPS3_SIGNATURE_LEN   5

#pragma pack(1)
typedef struct {
   char anchor_string[SMBIOS_EPS3_SIGNATURE_LEN];  /* '_SM3_' */
   uint8_t checksum;
   uint8_t length;
   uint8_t major_version;
   uint8_t minor_version;
   uint8_t doc_rev;
   uint8_t entry_point_revision;
   uint8_t reserved;
   uint32_t table_max_length;
   uint64_t table_address;
} smbios_eps3;
#pragma pack()

#pragma pack(1)
typedef struct {
   uint8_t type;
   uint8_t length;
   uint16_t handle;
} smbios_header;
#pragma pack()

typedef uint8_t smbios_string_id;

#pragma pack(1)
typedef struct {
   smbios_header header; // 2.0+
   smbios_string_id vendor; // 2.0+
   smbios_string_id bios_ver; // 2.0+
   uint16_t bios_seg; // 2.0+
   smbios_string_id bios_date; // 2.0+
   uint8_t bios_seg_count; // 2.0+
   uint64_t chars; // 2.0+
   uint16_t ext_chars; // 2.4+
   uint8_t major_release; // 2.4+
   uint8_t minor_release; // 2.4+
} smbios_type0;
#pragma pack()

typedef const uint8_t uuid_t[16];

#pragma pack(1)
typedef struct {
   smbios_header header; // 2.0+
   smbios_string_id manufacturer; // 2.0+
   smbios_string_id product_name; // 2.0+
   smbios_string_id version; // 2.0+
   smbios_string_id serial_number; // 2.0+
   uuid_t uuid; // 2.1+
   uint8_t wake_up_type; // 2.1+
   smbios_string_id sku; // 2.4+
   smbios_string_id family; // 2.4+
} smbios_type1;
#pragma pack()

#pragma pack(1)
typedef struct {
   smbios_header header; // 2.0+
   smbios_string_id manufacturer; // 2.0+
   uint8_t type; // 2.0+
   smbios_string_id version; // 2.0+
   smbios_string_id serial_number; // 2.0+
   smbios_string_id asset_tag; // 2.0+
   uint8_t boot_up_state; // 2.1+
   uint8_t power_supply_state; // 2.1+
   uint8_t thermal_state; // 2.1+
   uint8_t security_status; // 2.1+
   uint32_t oem_defined; // 2.3+
   uint8_t height; // 2.3+
   uint8_t number_of_power_cords; // 2.3+
   uint8_t element_count; // 2.3+
   uint8_t element_length; // 2.3+
   uint8_t elements_data[]; // 2.3+ [element_count * element_length]
   // smbios_string_id sku; // 2.7+
} smbios_type3;
#pragma pack()

#pragma pack(1)
typedef struct {
   smbios_header header;
   uint8_t count;
} smbios_type11;
#pragma pack()

typedef union {
   uint8_t *raw_bytes;
   smbios_header *header;
   smbios_type0 *type0;
   smbios_type1 *type1;
   smbios_type3 *type3;
   smbios_type11 *type11;
} smbios_entry;

typedef struct {
   size_t length;
   key_value_t* entries;
} oem_strings_t;

#endif /* !SM_BIOS_H_ */

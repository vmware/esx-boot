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
   smbios_header header;
   smbios_string_id vendor;
   smbios_string_id bios_ver;
   uint16_t bios_seg;
   smbios_string_id bios_date;
   uint8_t bios_seg_count;
   uint64_t chars;
   uint16_t ext_chars;
   uint8_t major_release;
   uint8_t minor_release;
} smbios_type0;
#pragma pack()

#pragma pack(1)
typedef struct {
   smbios_header header;
   smbios_string_id manufacturer;
   smbios_string_id product_name;
   smbios_string_id version;
   smbios_string_id serial_number;
   uint8_t uuid[16];
   uint8_t wake_up_type;
   smbios_string_id sku;
   smbios_string_id family;
} smbios_type1;
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
   smbios_type11 *type11;
} smbios_entry;

typedef struct {
   size_t length;
   char** names;
   key_value_t* entries;
} oem_strings_t;

#endif /* !SM_BIOS_H_ */

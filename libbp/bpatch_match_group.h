/*******************************************************************************
 * Copyright (c) 2022 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

#ifndef __BPATCH_MATCH_GROUP_H__
#define __BPATCH_MATCH_GROUP_H__

#include <stdint.h>
#include <bpatch_match_grp_arch.h>

/*
 * Enum values for each matching process supported
 */
typedef enum {
   MATCH_ARM_SYS_REG = 1,
   MATCH_OEM_ID_ACPI,
} type_value_t;

/*
 * Structure used for identifying the process type
 * used for matching process and needed parameter value
 */
typedef struct {
   type_value_t type;
   union {
      sysRegId_t mrsValue;
      char acpiTableSig[4];
   };
} match_type_t;


/*
 * GroupId value structure
 */
typedef struct {
   uint32_t patchGroupValue;
} patch_group_t;


/*
 * Register based matching process parameter
 */
typedef struct {
   uint64_t regMask;
   uint64_t regValue;
} register_process_t;


/*
 * ACPI based matching process parameter
 */
typedef struct {
   char oemTableId[8];
   char oemId[6];
   uint8_t sizeOemTblId;
   uint8_t sizeOemId;
} acpi_process_t;


/*
 * Top structure used for describing the matching process
 */
typedef struct {
   patch_group_t patchGroup;
   match_type_t  matchPatchGroupType;
   union {
      register_process_t registerProcess;
      acpi_process_t acpiProcess;
   };
} BPMatchPatchDesc_t;

#endif

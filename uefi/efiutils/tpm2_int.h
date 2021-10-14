/*******************************************************************************
 * Copyright (c) 2021 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * tpm2_int.h -- TPM 2 internal helpers.
 */

#ifndef TPM2_INT_H_
#define TPM2_INT_H_

#include "efi_private.h"


/*
 * Structures used for marshal/unmarshal of TPM commands.
 *
 * See Trusted Platform Module Library Part 3: Commands, Family “2.0”,
 * Level 00 Revision 01.38, September 29, 2016.
 */

// 31.6 TPM2_NV_ReadPublic

typedef struct {
   TPM2_COMMAND_HEADER hdr;
   TPMI_RH_NV_INDEX nvIndex;
} TPM2_NV_READPUBLIC_COMMAND;

typedef struct {
   TPM2_RESPONSE_HEADER hdr;
   TPM2B_NV_PUBLIC nvPublic;
   TPM2B_NAME nvName;
} TPM2_NV_READPUBLIC_RESPONSE;

// Section 31.13: TPM2_NV_Read

typedef struct {
   TPM2_COMMAND_HEADER hdr;
   TPMI_RH_NV_AUTH authHandle;
   TPMI_RH_NV_INDEX nvIndex;
   UINT32 authSize;
   TPMS_AUTH_COMMAND auth;
   UINT16 size;
   UINT16 offset;
} TPM2_NV_READ_COMMAND;

typedef struct {
   TPM2_RESPONSE_HEADER hdr;
   TPM2B_MAX_NV_BUFFER data;
} TPM2_NV_READ_RESPONSE;


uint32_t tpm2_marshal_nv_read(const TPM2_NV_READ_COMMAND *nvRead,
                              uint8_t *buffer, uint32_t size);
uint32_t tpm2_unmarshal_nv_read(const uint8_t *buffer, uint32_t size,
                                TPM2_NV_READ_RESPONSE *nvRead);

uint32_t tpm2_marshal_nv_readpublic(const TPM2_NV_READPUBLIC_COMMAND *nvRead,
                                    uint8_t *buffer, uint32_t size);
uint32_t tpm2_unmarshal_nv_readpublic(const uint8_t *buffer, uint32_t size,
                                      TPM2_NV_READPUBLIC_RESPONSE *nvRead);

EFI_STATUS tpm2_nv_read(uint32_t index, uint16_t size, uint8_t *buffer);
EFI_STATUS tpm2_nv_read_size(uint32_t index, uint16_t *size);

#endif /* !TPM2_INT_H_ */

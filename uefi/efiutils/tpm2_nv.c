/*******************************************************************************
 * Copyright (c) 2021 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * tpm2_nv.c -- TPM 2 NV memory access.
 */

#include "efi_private.h"
#include "tpm2_int.h"

/*
 * See Trusted Platform Module Library Part 3: Commands, Family "2.0",
 * Level 00 Revision 01.38, September 29, 2016, Section 4.4 Return Code
 * Alias
 */
#define RC_NV_ReadPublic_nvIndex    (TPM_RC_H + TPM_RC_1)
#define RC_NV_Read_nvIndex          (TPM_RC_H + TPM_RC_2)


/*-- tpm2_nv_read_size ---------------------------------------------------------
 *
 *      Read the size of an NV index.
 *
 *      See Trusted Platform Module Library Part 3: Commands, Family
 *      "2.0", Level 00 Revision 01.38, September 29, 2016, 31.13
 *      TPM2_NV_Read
 *
 * Parameters
 *      IN index:  The NV index to check.
 *      OUT size:  The size data at index.
 *
 * Results
 *      EFI_SUCCESS, or an error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS tpm2_nv_read_size(uint32_t index,
                             uint16_t *size)
{
   TPM2_NV_READPUBLIC_COMMAND in = {};
   TPM2_NV_READPUBLIC_RESPONSE out = {};
   uint8_t data[MAX(sizeof in, sizeof out)] = {};
   uint32_t dataSize;
   EFI_STATUS status;

   in.hdr.tag = TPM_ST_NO_SESSIONS;
   in.hdr.commandCode = TPM_CC_NV_ReadPublic;
   in.nvIndex = index;

   dataSize = tpm2_marshal_nv_readpublic(&in, data, sizeof data);
   if (dataSize == 0) {
      return EFI_BUFFER_TOO_SMALL;
   }

   status = tcg2_submit_command(data, dataSize, data, sizeof data);
   if (status != EFI_SUCCESS) {
      return status;
   }

   dataSize = tpm2_unmarshal_nv_readpublic(data, sizeof out, &out);
   if (dataSize == 0) {
      return EFI_BUFFER_TOO_SMALL;
   }

   if (out.hdr.responseCode != TPM_RC_SUCCESS) {
      switch (out.hdr.responseCode) {
      case TPM_RC_HANDLE + RC_NV_ReadPublic_nvIndex:
         return EFI_NOT_FOUND;
      case TPM_RC_VALUE + RC_NV_ReadPublic_nvIndex:
         return EFI_INVALID_PARAMETER;
      default:
         Log(LOG_ERR, "TPM NV read public failure: %x", out.hdr.responseCode);
         return EFI_DEVICE_ERROR;
      }
   }

   *size = out.nvPublic.nvPublic.dataSize;
   return EFI_SUCCESS;
}

/*-- tpm2_nv_read --------------------------------------------------------------
 *
 *      Read the value of an NV index.
 *
 *      This method only supports reading indexes that were created with
 *      TPMA_NV_AUTHREAD, where no authorization is required.
 *
 *      See Trusted Platform Module Library Part 3: Commands, Family
 *      "2.0", Level 00 Revision 01.38, September 29, 2016, 31.13
 *      TPM2_NV_Read
 *
 * Parameters
 *      IN index:    The NV index to read.
 *      IN size:     The size data to read in bytes.
 *      OUT buffer:  The result buffer.
 *
 * Results
 *      EFI_SUCCESS, or an error status.
 *----------------------------------------------------------------------------*/
EFI_STATUS tpm2_nv_read(uint32_t index,
                        uint16_t size,
                        uint8_t *buffer)
{
   TPM2_NV_READ_COMMAND in = {};
   TPM2_NV_READ_RESPONSE out = {};
   uint8_t data[MAX(sizeof in, sizeof out)] = {};
   uint32_t dataSize;
   EFI_STATUS status;

   in.hdr.tag = TPM_ST_SESSIONS;
   in.hdr.commandCode = TPM_CC_NV_Read;
   in.authHandle = index;
   in.auth.sessionHandle = TPM_RS_PW;
   in.nvIndex = index;
   in.size = size;
   in.offset = 0;

   dataSize = tpm2_marshal_nv_read(&in, data, sizeof data);
   if (dataSize == 0) {
      return EFI_BUFFER_TOO_SMALL;
   }

   status = tcg2_submit_command(data, dataSize, data, sizeof data);
   if (status != EFI_SUCCESS) {
      return status;
   }

   dataSize = tpm2_unmarshal_nv_read(data, sizeof out, &out);
   if (dataSize == 0) {
      return EFI_BUFFER_TOO_SMALL;
   }

   if (out.hdr.responseCode != TPM_RC_SUCCESS) {
      switch (out.hdr.responseCode) {
      case TPM_RC_HANDLE + RC_NV_Read_nvIndex:
         return EFI_NOT_FOUND;
      case TPM_RC_VALUE + RC_NV_Read_nvIndex:
         return EFI_INVALID_PARAMETER;
      case TPM_RC_NV_AUTHORIZATION:
         return EFI_ACCESS_DENIED;
      case TPM_RC_NV_LOCKED:
         return EFI_ACCESS_DENIED;
      case TPM_RC_NV_UNINITIALIZED:
         return EFI_NOT_READY;
      case TPM_RC_NV_RANGE:
         return EFI_BAD_BUFFER_SIZE;
      default:
         Log(LOG_ERR, "TPM NV read failure: %x", out.hdr.responseCode);
         return EFI_DEVICE_ERROR;
      }
   }

   if (out.data.size != size) {
      return EFI_BAD_BUFFER_SIZE;
   }

   memcpy(buffer, out.data.buffer, size);
   return EFI_SUCCESS;
}

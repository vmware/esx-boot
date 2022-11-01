/*******************************************************************************
 * Copyright (c) 2021 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * tpm2_marshal.c -- TPM 2 structure marshalling.
 */

#include "efi_private.h"
#include "tpm2_int.h"


#define BE16(val) __builtin_bswap16(val)
#define BE32(val) __builtin_bswap32(val)


typedef struct {
   UINT16 size;
   BYTE buffer[];
} TPM2B;

#define marshal_TPM2B_DIGEST(buffer, size, next, tpm2b) \
   marshal_TPM2B(buffer, size, next, (const TPM2B *)tpm2b)
#define marshal_TPM2B_NONCE(buffer, size, next, tpm2b) \
   marshal_TPM2B(buffer, size, next, (const TPM2B *)tpm2b)
#define marshal_TPM2B_AUTH(buffer, size, next, tpm2b) \
   marshal_TPM2B(buffer, size, next, (const TPM2B *)tpm2b)

#define unmarshal_TPM2B_DIGEST(buffer, size, next, tpm2b, tpm2bSize) \
   unmarshal_TPM2B(buffer, size, next, (TPM2B *)tpm2b, tpm2bSize)
#define unmarshal_TPM2B_MAX_NV_BUFFER(buffer, size, next, tpm2b, tpm2bSize) \
   unmarshal_TPM2B(buffer, size, next, (TPM2B *)tpm2b, tpm2bSize)
#define unmarshal_TPM2B_NAME(buffer, size, next, tpm2b, tpm2bSize) \
   unmarshal_TPM2B(buffer, size, next, (TPM2B *)tpm2b, tpm2bSize)


#define CHECK_OVERFLOW(...) \
   if (!marshal_checkoverflow(__VA_ARGS__)) { \
      return false; \
   }


/*-- marshal_checkoverflow -----------------------------------------------------
 *
 *      Check that we are not reading over buffer boundaries.
 *
 * Parameters
 *      IN buffer:    The command buffer start.
 *      IN size:      The command buffer total size.
 *      IN next:      The next used location in the command buffer.
 *      IN nextSize:  The size of the next data.
 *
 * Results
 *      True if no overflow, and false when overflow detected.
 *----------------------------------------------------------------------------*/
static bool marshal_checkoverflow(const uint8_t *buffer,
                                  uint32_t size,
                                  const uint8_t *next,
                                  uint32_t nextSize)
{
   if (buffer > next) {
      return false;
   }
   return (next + nextSize) <= (buffer + size);
}

/*-- marshal_UINT8 -------------------------------------------------------------
 *
 *      Marshal a 8 bit unsigned integer.
 *
 * Parameters
 *      IN buffer:    The command buffer start.
 *      IN size:      The command buffer total size.
 *      IN/OUT next:  The next used location in the command buffer.
 *      IN value:     The value to add at next.
 *
 * Results
 *      True on success, false otherwise.
 *----------------------------------------------------------------------------*/
static bool marshal_UINT8(uint8_t *buffer,
                          uint32_t size,
                          uint8_t **next,
                          uint8_t value)
{
   CHECK_OVERFLOW(buffer, size, *next, sizeof (value));

   *(*next) = value;
   *next += sizeof (value);
   return true;
}

/*-- marshal_UINT16 ------------------------------------------------------------
 *
 *      Marshal a 16 bit unsigned integer.
 *
 * Parameters
 *      IN buffer:    The command buffer start.
 *      IN size:      The command buffer total size.
 *      IN/OUT next:  The next used location in the command buffer.
 *      IN value:     The value to add at next.
 *
 * Results
 *      True on success, false otherwise.
 *----------------------------------------------------------------------------*/
static bool marshal_UINT16(uint8_t *buffer,
                           uint32_t size,
                           uint8_t **next,
                           uint16_t value)
{
   CHECK_OVERFLOW(buffer, size, *next, sizeof (value));

   *((uint16_t *)*next) = BE16(value);
   *next += sizeof (value);
   return true;
}

/*-- unmarshal_UINT16 ----------------------------------------------------------
 *
 *      Unmarshal a 16 bit unsigned integer.
 *
 * Parameters
 *      IN buffer:    The command buffer start.
 *      IN size:      The command buffer total size.
 *      IN/OUT next:  The next used location in the command buffer.
 *      OUT value:    The value read from next.
 *
 * Results
 *      True on success, false otherwise.
 *----------------------------------------------------------------------------*/
static bool unmarshal_UINT16(const uint8_t *buffer,
                             uint32_t size,
                             const uint8_t **next,
                             uint16_t *value)
{
   CHECK_OVERFLOW(buffer, size, *next, sizeof (*value));

   *value = BE16(*(const uint16_t *)*next);
   *next += sizeof (*value);
   return true;
}

/*-- marshal_UINT32 ------------------------------------------------------------
 *
 *      Marshal a 32 bit unsigned integer.
 *
 * Parameters
 *      IN buffer:    The command buffer start.
 *      IN size:      The command buffer total size.
 *      IN/OUT next:  The next used location in the command buffer.
 *      IN value:     The value to add at next.
 *
 * Results
 *      True on success, false otherwise.
 *----------------------------------------------------------------------------*/
static bool marshal_UINT32(uint8_t *buffer,
                           uint32_t size,
                           uint8_t **next,
                           uint32_t value)
{
   CHECK_OVERFLOW(buffer, size, *next, sizeof (value));

   *((uint32_t *)*next) = BE32(value);
   *next += sizeof (value);
   return true;
}

/*-- unmarshal_UINT32 ----------------------------------------------------------
 *
 *      Unmarshal a 32 bit unsigned integer.
 *
 * Parameters
 *      IN buffer:    The command buffer start.
 *      IN size:      The command buffer total size.
 *      IN/OUT next:  The next used location in the command buffer.
 *      OUT value:    The value read from next.
 *
 * Results
 *      True on success, false otherwise.
 *----------------------------------------------------------------------------*/
static bool unmarshal_UINT32(const uint8_t *buffer,
                             uint32_t size,
                             const uint8_t **next,
                             uint32_t *value)
{
   CHECK_OVERFLOW(buffer, size, *next, sizeof (*value));

   *value = BE32(*(const uint32_t *)*next);
   *next += sizeof (*value);
   return true;
}

/*-- marshal_TPM2B -------------------------------------------------------------
 *
 *      Marshal a TPM2B_* object.
 *
 *      There are many TPM2B objects that all start with a 16 bit size
 *      field followed by data. This function is a helper for handling
 *      any such TPM2B object.
 *
 * Parameters
 *      IN buffer:    The command buffer start.
 *      IN size:      The command buffer total size.
 *      IN/OUT next:  The next used location in the command buffer.
 *      IN tpm2b:     The TPM2B to add at next.
 *
 * Results
 *      True on success, false otherwise.
 *----------------------------------------------------------------------------*/
static bool marshal_TPM2B(uint8_t *buffer,
                          uint32_t size,
                          uint8_t **next,
                          const TPM2B *tpm2b)
{
   if (tpm2b == NULL) {
      return marshal_UINT16(buffer, size, next, 0);
   }

   if (!marshal_UINT16(buffer, size, next, tpm2b->size)) {
      return false;
   }

   CHECK_OVERFLOW(buffer, size, *next, tpm2b->size);

   memcpy(*next, tpm2b->buffer, tpm2b->size);
   *next += tpm2b->size;
   return true;
}

/*-- unmarshal_TPM2B -----------------------------------------------------------
 *
 *      Unmarshal a TPM2B_* object.
 *
 * Parameters
 *      IN buffer:     The command buffer start.
 *      IN size:       The command buffer total size.
 *      IN/OUT next:   The next used location in the command buffer.
 *      OUT tpm2b:     The TPM2B read from next.
 *      IN tpm2bSize:  The buffer size available for the TPM2B.
 *
 * Results
 *      True on success, false otherwise.
 *----------------------------------------------------------------------------*/
static bool unmarshal_TPM2B(const uint8_t *buffer,
                            uint32_t size,
                            const uint8_t **next,
                            TPM2B *tpm2b,
                            uint32_t tpm2bSize)
{
   if (!unmarshal_UINT16(buffer, size, next, &tpm2b->size)) {
      return false;
   }

   CHECK_OVERFLOW(buffer, size, *next, tpm2b->size);

   if (tpm2b->size > tpm2bSize) {
      return false;
   }

   memcpy(tpm2b->buffer, *next, tpm2b->size);
   *next += tpm2b->size;
   return TRUE;
}

/*-- unmarshal_TPM2_RESPONSE_HEADER --------------------------------------------
 *
 *      Unmarshal a TPM2_RESPONSE_HEADER object.
 *
 * Parameters
 *      IN buffer:    The command buffer start.
 *      IN size:      The command buffer total size.
 *      IN/OUT next:  The next used location in the command buffer.
 *      OUT hdr:      The TPM2_RESPONSE_HEADER read from next.
 *
 * Results
 *      True on success, false otherwise.
 *----------------------------------------------------------------------------*/
static bool unmarshal_TPM2_RESPONSE_HEADER(const uint8_t *buffer,
                                           uint32_t size,
                                           const uint8_t **next,
                                           TPM2_RESPONSE_HEADER *hdr)
{
   return unmarshal_UINT16(buffer, size, next, &hdr->tag) &&
          unmarshal_UINT32(buffer, size, next, &hdr->paramSize) &&
          unmarshal_UINT32(buffer, size, next, &hdr->responseCode);
}

/*-- unmarshal_TPMS_NV_PUBLIC --------------------------------------------------
 *
 *      Unmarshal a TPMS_NV_PUBLIC object.
 *
 * Parameters
 *      IN buffer:     The command buffer start.
 *      IN size:       The command buffer total size.
 *      IN/OUT next:   The next used location in the command buffer.
 *      OUT nvPublic:  The TPMS_NV_PUBLIC read from next.
 *
 * Results
 *      True on success, false otherwise.
 *----------------------------------------------------------------------------*/
static bool unmarshal_TPMS_NV_PUBLIC(const uint8_t *buffer,
                                     uint32_t size,
                                     const uint8_t **next,
                                     TPMS_NV_PUBLIC *nvPublic)
{
   return unmarshal_UINT32(buffer, size, next, &nvPublic->nvIndex) &&
          unmarshal_UINT16(buffer, size, next, &nvPublic->nameAlg) &&
          unmarshal_UINT32(buffer, size, next,
                           (UINT32 *)(&nvPublic->attributes)) &&
          unmarshal_TPM2B_DIGEST(buffer, size, next, &nvPublic->authPolicy,
                                 sizeof nvPublic->authPolicy.buffer) &&
          unmarshal_UINT16(buffer, size, next, &nvPublic->dataSize);
}

/*-- unmarshal_TPM2B_NV_PUBLIC -------------------------------------------------
 *
 *      Unmarshal a TPM2B_NV_PUBLIC object.
 *
 * Parameters
 *      IN buffer:     The command buffer start.
 *      IN size:       The command buffer total size.
 *      IN/OUT next:   The next used location in the command buffer.
 *      OUT nvPublic:  The TPM2B_NV_PUBLIC read from next.
 *
 * Results
 *      True on success, false otherwise.
 *----------------------------------------------------------------------------*/
static bool unmarshal_TPM2B_NV_PUBLIC(const uint8_t *buffer,
                                      uint32_t size,
                                      const uint8_t **next,
                                      TPM2B_NV_PUBLIC *nvPublic)
{
   const uint8_t *nvPublicStart;

   if (!unmarshal_UINT16(buffer, size, next, &nvPublic->size)) {
      return false;
   }
   if (nvPublic->size == 0) {
      return true;
   }

   CHECK_OVERFLOW(buffer, size, *next, nvPublic->size);

   nvPublicStart = *next;
   if (!unmarshal_TPMS_NV_PUBLIC(nvPublicStart, nvPublic->size, next,
                                 &nvPublic->nvPublic)) {
      return false;
   }

   *next = nvPublicStart + nvPublic->size;
   return true;
}

/*-- marshal_TPM2_COMMAND_HEADER -----------------------------------------------
 *
 *      Marshal a TPM2_COMMAND_HEADER object.
 *
 *      The final command size cannot be known until the entire command
 *      has been marshalled. We leave a placeholder for the size, and a
 *      follow-up call to marshal_TPM2_COMMAND_HEADER_done will update
 *      the size.
 *
 * Parameters
 *      IN buffer:    The command buffer start.
 *      IN size:      The command buffer total size.
 *      IN/OUT next:  The next used location in the command buffer.
 *      IN hdr:       The TPM2_COMMAND_HEADER to add at next.
 *
 * Results
 *      True on success, false otherwise.
 *----------------------------------------------------------------------------*/
static bool marshal_TPM2_COMMAND_HEADER(uint8_t *buffer,
                                        uint32_t size,
                                        uint8_t **next,
                                        const TPM2_COMMAND_HEADER *hdr)
{
   return marshal_UINT16(buffer, size, next, hdr->tag) &&
          // We don't know the size yet, so leave a placeholder.
          marshal_UINT32(buffer, size, next, 0) &&
          marshal_UINT32(buffer, size, next, hdr->commandCode);
}

/*-- marshal_TPM2_COMMAND_HEADER_done ------------------------------------------
 *
 *      Complete the marshalling of the TPM2_COMMAND_HEADER object.
 *
 *      Update the paramSize in the command buffer header with the final
 *      size of the marshalled command.
 *
 * Parameters
 *      IN buffer:    The command buffer start.
 *      IN size:      The command buffer total size.
 *      IN/OUT next:  The next used location in the command buffer.
 *
 * Results
 *      True on success, false otherwise.
 *----------------------------------------------------------------------------*/
static bool marshal_TPM2_COMMAND_HEADER_done(uint8_t *buffer,
                                             uint32_t size,
                                             uint8_t *next)
{
   TPM2_COMMAND_HEADER *hdr = (TPM2_COMMAND_HEADER *)buffer;

   if (offsetof(TPM2_COMMAND_HEADER, paramSize) != 2 ||
       next < buffer + sizeof *hdr ||
       (uintptr_t)(next - buffer) > size) {
      return false;
   }

   hdr->paramSize = BE32(next - buffer);
   return true;
}

/*-- marshal_TPMS_AUTH_COMMAND -------------------------------------------------
 *
 *      Marshal a TPMS_AUTH_COMMAND object.
 *
 * Parameters
 *      IN buffer:    The command buffer start.
 *      IN size:      The command buffer total size.
 *      IN/OUT next:  The next used location in the command buffer.
 *      IN auth:      The TPMS_AUTH_COMMAND to add at next.
 *
 * Results
 *      True on success, false otherwise.
 *----------------------------------------------------------------------------*/
static bool marshal_TPMS_AUTH_COMMAND(uint8_t *buffer,
                                      uint32_t size,
                                      uint8_t **next,
                                      const TPMS_AUTH_COMMAND *auth)
{
   uint8_t *authStart;
   uint8_t *authSizeNext;
   uint32_t authSize;
   union {
      TPMA_SESSION t;
      UINT8 val;
   } sessionAttributes;

   authSizeNext = *next;
   if (!marshal_UINT32(buffer, size, next, 0)) {
      return false;
   }
   authStart = *next;

   sessionAttributes.t = auth->sessionAttributes;

   if (!marshal_UINT32(buffer, size, next, auth->sessionHandle) ||
       !marshal_TPM2B_NONCE(buffer, size, next, &auth->nonce) ||
       !marshal_UINT8(buffer, size, next, sessionAttributes.val) ||
       !marshal_TPM2B_AUTH(buffer, size, next, &auth->hmac)) {
      return false;
   }

   authSize = *next - authStart;
   return marshal_UINT32(buffer, size, &authSizeNext, authSize);
}

/*-- tpm2_marshal_nv_read ------------------------------------------------------
 *
 *      Marshal the NV read command.
 *
 * Parameters
 *      IN nvRead:   The read command.
 *      OUT buffer:  The command buffer start.
 *      IN size:     The command buffer total size.
 *
 * Results
 *      The size of the marshalled data, or 0 on failure.
 *----------------------------------------------------------------------------*/
uint32_t tpm2_marshal_nv_read(const TPM2_NV_READ_COMMAND *nvRead,
                              uint8_t *buffer,
                              uint32_t size)
{
   bool result;
   uint8_t *next = buffer;

   result = marshal_TPM2_COMMAND_HEADER(buffer, size, &next, &nvRead->hdr) &&
            marshal_UINT32(buffer, size, &next, nvRead->authHandle) &&
            marshal_UINT32(buffer, size, &next, nvRead->nvIndex) &&
            marshal_TPMS_AUTH_COMMAND(buffer, size, &next, &nvRead->auth) &&
            marshal_UINT16(buffer, size, &next, nvRead->size) &&
            marshal_UINT16(buffer, size, &next, nvRead->offset) &&
            marshal_TPM2_COMMAND_HEADER_done(buffer, size, next);
   return result ? next - buffer : 0;
}

/*-- tpm2_unmarshal_nv_read ----------------------------------------------------
 *
 *      Unmarshal the NV read response.
 *
 * Parameters
 *      IN buffer:   The command buffer start.
 *      IN size:     The command buffer total size.
 *      OUT nvRead:  The read command.
 *
 * Results
 *      The size of the unmarshalled data, or 0 on failure.
 *----------------------------------------------------------------------------*/
uint32_t tpm2_unmarshal_nv_read(const uint8_t *buffer,
                                uint32_t size,
                                TPM2_NV_READ_RESPONSE *nvRead)
{
   bool result;
   const uint8_t *next = buffer;
   uint32_t parameterSize;

   result = unmarshal_TPM2_RESPONSE_HEADER(buffer, size, &next, &nvRead->hdr) &&
            unmarshal_UINT32(buffer, size, &next, &parameterSize) &&
            unmarshal_TPM2B_MAX_NV_BUFFER(buffer, size, &next, &nvRead->data,
                                          sizeof nvRead->data.buffer);
   return result ? next - buffer : 0;
}

/*-- tpm2_marshal_nv_readpublic ------------------------------------------------
 *
 *      Marshal the NV read public command.
 *
 * Parameters
 *      IN nvRead:   The read public command.
 *      OUT buffer:  The command buffer start.
 *      IN size:     The command buffer total size.
 *
 * Results
 *      The size of the marshalled data, or 0 on failure.
 *----------------------------------------------------------------------------*/
uint32_t tpm2_marshal_nv_readpublic(const TPM2_NV_READPUBLIC_COMMAND *nvRead,
                                    uint8_t *buffer,
                                    uint32_t size)
{
   bool result;
   uint8_t *next = buffer;

   result = marshal_TPM2_COMMAND_HEADER(buffer, size, &next, &nvRead->hdr) &&
            marshal_UINT32(buffer, size, &next, nvRead->nvIndex) &&
            marshal_TPM2_COMMAND_HEADER_done(buffer, size, next);
   return result ? next - buffer : 0;
}

/*-- tpm2_unmarshal_nv_readpublic ----------------------------------------------
 *
 *      Unmarshal the NV read public response.
 *
 * Parameters
 *      IN buffer:   The command buffer start.
 *      IN size:     The command buffer total size.
 *      OUT nvRead:  The read public command.
 *
 * Results
 *      The size of the unmarshalled data, or 0 on failure.
 *----------------------------------------------------------------------------*/
uint32_t
tpm2_unmarshal_nv_readpublic(const uint8_t *buffer,
                             uint32_t size,
                             TPM2_NV_READPUBLIC_RESPONSE *nvRead)
{
   bool result;
   const uint8_t *next = buffer;

   result = unmarshal_TPM2_RESPONSE_HEADER(buffer, size, &next, &nvRead->hdr);
   if (result && nvRead->hdr.responseCode == TPM_RC_SUCCESS) {
      result = unmarshal_TPM2B_NV_PUBLIC(buffer, size, &next,
                                         &nvRead->nvPublic) &&
               unmarshal_TPM2B_NAME(buffer, size, &next, &nvRead->nvName,
                                    sizeof nvRead->nvName.name);
   }
   return result ? next - buffer : 0;
}

/*******************************************************************************
 * Copyright (c) 2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * tcg2.c -- EFI TCG2 protocol support.
 */

#include "efi_private.h"

static EFI_TCG2_PROTOCOL *tcg2 = NULL;


/*-- tcg2_get_digest_size ------------------------------------------------------
 *
 *      Determine the size of the cryptographic hash value in the
 *      specified TPMT_HA digest structure.
 *
 *      Note that we can remove this hard-coded digest list by parsing
 *      the digest sizes in the log header.
 *
 * Parameters
 *      IN digest: A TPMT_HA digest
 *
 * Results
 *      The size of the digest
 *----------------------------------------------------------------------------*/
static uint32_t tcg2_get_digest_size(const TPMT_HA *digest)
{
   switch (digest->hashAlg) {
   case TPM_ALG_SHA1:
      return sizeof digest->digest.sha1;
   case TPM_ALG_SHA256:
      return sizeof digest->digest.sha256;
   case TPM_ALG_SHA384:
      return sizeof digest->digest.sha384;
   case TPM_ALG_SHA512:
      return sizeof digest->digest.sha512;
   case TPM_ALG_SM3_256:
      return sizeof digest->digest.sm3_256;
   }

   return 0;
}

/*-- tcg2_get_tcg_event2_size --------------------------------------------------
 *
 *      Determine the size of a TCG_PCR_EVENT2 event structure.
 *
 *      The TCG_PCR_EVENT2 format is packed and may include multiple
 *      digests of different types and sizes. We scan through the
 *      structure to find the end and subtract that from the start
 *      address to get the total size.
 *
 * Parameters
 *      IN address: The address of a TCG_PCR_EVENT2 event structure
 *
 * Results
 *      The size of TCG_PCR_EVENT2 entry in the log
 *----------------------------------------------------------------------------*/
static uint32_t tcg2_get_tcg_event2_size(EFI_PHYSICAL_ADDRESS address)
{
   TCG_PCR_EVENT2 *event = (TCG_PCR_EVENT2 *)(UINTN)address;
   TPML_DIGEST_VALUES *digests = &event->Digest;
   uint8_t *offset;

   offset = (uint8_t *)&digests->digests[0];
   for (unsigned i = 0; i < digests->count; i++) {
      TPMT_HA *digest = (TPMT_HA *)offset;
      uint32_t digestSize = tcg2_get_digest_size(digest);
      if (digestSize == 0) {
         Log(LOG_WARNING, "Unknown event log algorithm: %#x", digest->hashAlg);
         return 0;
      }
      offset += sizeof(digest->hashAlg) + tcg2_get_digest_size(digest);
   }

   // A uint32_t event size is the next field followed by the event data
   offset += sizeof (uint32_t) + *(uint32_t *)offset;

   return (UINTN)offset - address;
}

/*-- tcg2_get_event_log_header_size --------------------------------------------
 *
 *      The EFI_TCG2_EVENT_LOG_FORMAT_TCG_2 event log format includes a
 *      header entry of type TCG_PCR_EVENT. Determine the size of the
 *      TCG_PCR_EVENT entry.
 *
 * Parameters
 *      IN address: The address of the start of the event log
 *
 * Results
 *      The size of the event log header in bytes
 *----------------------------------------------------------------------------*/
static uint32_t tcg2_get_event_log_header_size(EFI_PHYSICAL_ADDRESS address)
{
   const TCG_PCR_EVENT *header = (const TCG_PCR_EVENT *)(UINTN)address;
   return sizeof (TCG_PCR_EVENT) + header->EventSize;
}

/*-- tcg2_get_event_log --------------------------------------------------------
 *
 *      Retrieve the address and size of the TCG event log in the
 *      EFI_TCG2_EVENT_LOG_FORMAT_TCG_2 format.
 *
 *      Failure is reported if the EFI_TCG2_EVENT_LOG_FORMAT_TCG_2
 *      format is not available, if the log is truncated, or if the log
 *      contains a digest type that is unknown.
 *
 *      See TCG EFI Protocol Specification, Family “2.0”, Level 00
 *      Revision 00.13, March 30, 2016, Section 6.5:
 *      EFI_TCG2_PROTOCOL.GetEventLog
 *
 * Parameters
 *      OUT address:    The address of the event log
 *      OUT size:       The size in bytes of the event log
 *      OUT truncated:  The log is truncated due to space limitations.
 *
 * Results
 *      EFI_SUCCESS, or an UEFI if the event log is not avaialble.
 *----------------------------------------------------------------------------*/
EFI_STATUS tcg2_get_event_log(const uint8_t **address,
                              uint32_t *size,
                              bool *truncatedOut)
{
   EFI_STATUS Status;
   EFI_PHYSICAL_ADDRESS location;
   EFI_PHYSICAL_ADDRESS lastEntry;
   BOOLEAN truncated;
   uint32_t lastEntrySize;

   if (tcg2 == NULL) {
      // EFI_TCG2_PROTOCOL is not available.
      return EFI_NOT_FOUND;
   }

   Status = tcg2->GetEventLog(tcg2, EFI_TCG2_EVENT_LOG_FORMAT_TCG_2,
                              &location, &lastEntry, &truncated);
   if (EFI_ERROR(Status)) {
      // The requested log format is not supported.
      EFI_ASSERT(Status == EFI_INVALID_PARAMETER);
      return EFI_NOT_FOUND;
   }

   /*
    * The location being 0 means that there is no TPM.
    * The last entry being 0 means that the log is empty.
    * The last entry should never come before the location.
    */
   if (location == 0 || lastEntry == 0 || lastEntry < location) {
      return EFI_NOT_FOUND;
   }

   /*
    * If the event log has no measured entries (only the header entry),
    * the lastEntry is the same as location. This is the only case where
    * the last entry points a structure of type TCG_PCR_EVENT instead of
    * the crypto agile format, TCG_PCR_EVENT2.
    */
   if (lastEntry == location) {
      // Log contains only the header
      lastEntry += tcg2_get_event_log_header_size(lastEntry);
      lastEntrySize = 0;
   } else {
      lastEntrySize = tcg2_get_tcg_event2_size(lastEntry);
      if (lastEntrySize == 0) {
         // Failed to parse the last entry
         return EFI_NOT_FOUND;
      }
   }

   *address = (const uint8_t *)(UINTN)location;
   *size = lastEntry + lastEntrySize - location;
   *truncatedOut = truncated != 0;

   return EFI_SUCCESS;
}

/*-- tcg2_init -----------------------------------------------------------------
 *
 *      Initialize the TCG2 protocol.
 *
 * Parameters
 *      None
 *
 * Results
 *      None
 *----------------------------------------------------------------------------*/
void tcg2_init(void)
{
   EFI_GUID tcg2_protocol = EFI_TCG2_PROTOCOL_GUID;
   EFI_STATUS Status;

   Status = LocateProtocol(&tcg2_protocol, (void **)&tcg2);
   if (EFI_ERROR(Status)) {
      Log(LOG_WARNING, "TCG2 protocol not available");
      tcg2 = NULL;
   }

   Log(LOG_DEBUG, "TCG2 protocol initialized");
}

/*******************************************************************************
 * Copyright (c) 2020-2022 VMware, Inc.  All rights reserved.
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
 *      // XXX Get sizes from the TCG_PCR_EVENT header.
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
 *      IN event: TCG_PCR_EVENT2 event structure
 *
 * Results
 *      The size of TCG_PCR_EVENT2 entry in the log
 *----------------------------------------------------------------------------*/
static uint32_t tcg2_get_tcg_event2_size(const TCG_PCR_EVENT2 *event)
{
   const TPML_DIGEST_VALUES *digests = &event->Digest;
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

   return (uintptr_t)offset - (uintptr_t)event;
}

/*-- tcg2_get_event_log_header_size --------------------------------------------
 *
 *      The EFI_TCG2_EVENT_LOG_FORMAT_TCG_2 event log format includes a
 *      header entry of type TCG_PCR_EVENT. Determine the size of the
 *      TCG_PCR_EVENT entry.
 *
 * Parameters
 *      IN header: The address of the start of the event log
 *
 * Results
 *      The size of the event log header in bytes
 *----------------------------------------------------------------------------*/
static uint32_t tcg2_get_event_log_header_size(const TCG_PCR_EVENT *header)
{
   return sizeof (TCG_PCR_EVENT) + header->EventSize;
}

/*-- tcg2_final_events_size ----------------------------------------------------
 *
 *      Get the length of the final events list.
 *
 *      See TCG EFI Protocol Specification, Family “2.0”, Level 00
 *      Revision 00.13, March 30, 2016, Section 7: Log entries after Get
 *      Event Log service
 *
 * Parameters
 *      OUT firstEvent: The first event in the final events list
 *
 * Results
 *      The size of the final events list.
 *----------------------------------------------------------------------------*/
static uint32_t tcg2_final_events_size(const TCG_PCR_EVENT2 **firstEvent)
{
   EFI_TCG2_FINAL_EVENTS_TABLE *table;
   uint8_t *next;
   uint32_t totalSize;
   unsigned int i;
   int status;

   status = get_tcg2_final_events((void **)&table);
   if (status != ERR_SUCCESS) {
      return 0;
   }

   if (table->Version == 0) {
      Log(LOG_WARNING, "Unknown TCG2 final events table version");
      return 0;
   }

   next = (uint8_t *)table + sizeof table->Version +
          sizeof table->NumberOfEvents;
   totalSize = 0;

   for (i = 0; i < table->NumberOfEvents; i++) {
      TCG_PCR_EVENT2 *event = (TCG_PCR_EVENT2 *)(next + totalSize);
      uint32_t size;

      size = tcg2_get_tcg_event2_size(event);
      if (size == 0) {
         Log(LOG_ERR, "Invalid TCG2 final event data");
         return 0;
      }

      if (i == 0) {
         *firstEvent = event;
      }
      totalSize += size;
   }

   return totalSize;
}

/*-- tcg2_adjust_event_log_size -----------------------------------------------
 *
 *      Adjust the event log size to remove redundant entries which are
 *      already included in the final events table.
 *
 *      The firmware will add final events entries after the first call
 *      to GetEventLog is made. As a result, it's possible for the final
 *      events table to logically overlap the GetEventLog log when
 *      multiple calls are made to GetEventLog.
 *
 *           |-------------------------|
 *           |   GetEventLog Events    |
 *           |-------------------------|
 *                       |-----------------|
 *                       |  Final Events   |
 *                       |-----------------|
 *                                     ^
 *                               EBS was called.
 *                       \_____________/
 *                               |
 *                       Redundant Events
 *
 *      Given that the OS will always need to check the final events
 *      table to get a complete log, we can avoid returning the
 *      redundant entries here and rely on the OS to append the final
 *      events.
 *
 * Parameters
 *      IN address: The address of the event log
 *      IN/OUT size: The size in bytes of the event log
 *
 * Results
 *      None
 *----------------------------------------------------------------------------*/
static void tcg2_adjust_event_log_size(const uint8_t *address,
                                       uint32_t *size)
{
   uint32_t finalEventsSize;
   uint32_t headerSize;
   uint32_t adjustedSize;
   const TCG_PCR_EVENT2 *adjustedNextEvent;
   uint32_t adjustedNextEventSize;
   const TCG_PCR_EVENT2 *firstFinalEvent;
   uint32_t firstFinalEventSize;

   finalEventsSize = tcg2_final_events_size(&firstFinalEvent);
   if (finalEventsSize == 0) {
      return;
   }

   /* Final events can't be larger that the total events. */
   headerSize = tcg2_get_event_log_header_size((const TCG_PCR_EVENT *)address);
   if (finalEventsSize > *size - headerSize) {
      return;
   }

   /* The final events overlap the end of the total events. */
   adjustedSize = *size - finalEventsSize;

   adjustedNextEvent = (TCG_PCR_EVENT2 *)(address + adjustedSize);
   adjustedNextEventSize = tcg2_get_tcg_event2_size(adjustedNextEvent);
   firstFinalEventSize = tcg2_get_tcg_event2_size(firstFinalEvent);

   /* Verify the adjusted next event matches the first final event. */
   if (adjustedNextEventSize == 0 ||
       adjustedNextEventSize != firstFinalEventSize ||
       memcmp(adjustedNextEvent, firstFinalEvent, firstFinalEventSize) != 0) {
      return;
   }

   *size = adjustedSize;
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
 *      EFI_SUCCESS, or an error if the event log is not available.
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
      const TCG_PCR_EVENT *header = (const TCG_PCR_EVENT *)(UINTN)lastEntry;
      lastEntry += tcg2_get_event_log_header_size(header);
      lastEntrySize = 0;
   } else {
      TCG_PCR_EVENT2 *event = (TCG_PCR_EVENT2 *)(UINTN)lastEntry;
      lastEntrySize = tcg2_get_tcg_event2_size(event);
      if (lastEntrySize == 0) {
         // Failed to parse the last entry
         return EFI_NOT_FOUND;
      }
   }


   *address = (const uint8_t *)(UINTN)location;
   *size = lastEntry + lastEntrySize - location;
   *truncatedOut = truncated != 0;

   tcg2_adjust_event_log_size(*address, size);

   return EFI_SUCCESS;
}

/*-- tcg2_log_extend_event -----------------------------------------------------
 *
 *      Extend the TPM with the provided event data.
 *
 *      Note that in some cases the event may be extended into the TPM
 *      but the log entry may be missing. For example, if the log has
 *      run out of space.
 *
 *      XXX HashLogExtendEvent may be too slow for measuring all
 *      modules. If that the case we may need to run the hash algorithm
 *      ourselves using crypto64.
 *
 *      See TCG EFI Protocol Specification, Family “2.0”, Level 00
 *      Revision 00.13, March 30, 2016, Section 6.6:
 *      EFI_TCG2_PROTOCOL.HashLogExtendEvent
 *
 * Parameters
 *      IN pcrIndex:   Index of the PCR that will be extended
 *      IN data:       Address of the data to be hashed
 *      IN dataSize:   Size in bytes of data to be hashed
 *      IN eventType:  Identifier of the type of event
 *      IN event:      Data included in the event log
 *      IN eventSize:  Size in bytes of data included in the event log
 *
 * Results
 *      EFI_SUCCESS, or an error extending fails.
 *----------------------------------------------------------------------------*/
EFI_STATUS tcg2_log_extend_event(uint32_t pcrIndex,
                                 const uint8_t *data,
                                 uint64_t dataSize,
                                 uint32_t eventType,
                                 const uint8_t *event,
                                 uint64_t eventSize)
{
   EFI_TCG2_EVENT *tcg2Event;
   const uint32_t tcg2EventSize = sizeof(EFI_TCG2_EVENT) + eventSize;
   EFI_STATUS Status;

   if (tcg2 == NULL) {
      return EFI_NOT_STARTED;
   }

   tcg2Event = sys_malloc(tcg2EventSize);
   if (tcg2Event == NULL) {
      return EFI_OUT_OF_RESOURCES;
   }

   tcg2Event->Size = tcg2EventSize;
   tcg2Event->Header.HeaderSize = sizeof(EFI_TCG2_EVENT_HEADER);
   tcg2Event->Header.HeaderVersion = EFI_TCG2_EVENT_HEADER_VERSION;
   tcg2Event->Header.PCRIndex = pcrIndex;
   tcg2Event->Header.EventType = eventType;
   memcpy(&tcg2Event->Event[0], event, eventSize);

   Status = tcg2->HashLogExtendEvent(tcg2, 0,
                                     (EFI_PHYSICAL_ADDRESS)(UINTN)data,
                                     dataSize, tcg2Event);
   sys_free(tcg2Event);

   /*
    * Ignore log full errors. This error condition will be detected by
    * the OS as a truncated event log, and remote attestation may fail.
    */
   if (Status == EFI_VOLUME_FULL) {
      Log(LOG_WARNING, "Event log full while measuring event type %u to PCR %u",
          eventType, pcrIndex);
      Status = EFI_SUCCESS;
   }

   return Status;
}

/*-- tcg2_submit_command -------------------------------------------------------
 *
 *      Submit a command to the TPM.
 *
 *      See TCG EFI Protocol Specification, Family “2.0”, Level 00
 *      Revision 00.13, March 30, 2016, Section 6.7:
 *      EFI_TCG2_PROTOCOL.SubmitCommand
 *
 * Parameters
 *      IN input:       Input data block
 *      IN inputSize:   Size of the input data block
 *      OUT output:     Output data block
 *      IN outputSize:  Size of the output data block
 *
 * Results
 *      EFI_SUCCESS, or an error extending fails.
 *----------------------------------------------------------------------------*/
EFI_STATUS tcg2_submit_command(uint8_t *input,
                               uint32_t inputSize,
                               uint8_t *output,
                               uint32_t outputSize)
{
   if (tcg2 == NULL) {
      return EFI_NOT_STARTED;
   }

   return tcg2->SubmitCommand(tcg2, inputSize, input, outputSize, output);
}

/*-- tcg2_init -----------------------------------------------------------------
 *
 *      Initialize the TCG2 protocol.
 *
 * Parameters
 *      None
 *
 * Results
 *      True if TCG2 protocol is available, false otherwise.
 *----------------------------------------------------------------------------*/
bool tcg2_init(void)
{
   EFI_GUID tcg2_protocol = EFI_TCG2_PROTOCOL_GUID;
   EFI_TCG2_PROTOCOL *tcg2Local = NULL;
   EFI_TCG2_BOOT_SERVICE_CAPABILITY capability;
   EFI_STATUS Status;

   Status = LocateProtocol(&tcg2_protocol, (void **)&tcg2Local);
   if (EFI_ERROR(Status)) {
      Log(LOG_WARNING, "TCG2 protocol not available: %zx", Status);
      return false;
   }

   capability.Size = sizeof (capability);
   Status = tcg2Local->GetCapability(tcg2Local, &capability);
   if (EFI_ERROR(Status)) {
      Log(LOG_ERR, "Failed to query TPM capability: %zx", Status);
      return false;
   }

   if (!capability.TPMPresentFlag) {
      Log(LOG_DEBUG, "No TPM present");
      return false;
   }

   if ((capability.SupportedEventLogs & EFI_TCG2_EVENT_LOG_FORMAT_TCG_2) == 0) {
      Log(LOG_WARNING, "Required TCG2 event log format not supported");
      return false;
   }

   if ((capability.ActivePcrBanks & EFI_TCG2_BOOT_HASH_ALG_SHA256) == 0) {
      Log(LOG_WARNING, "Required TPM PCR bank not enabled: SHA256");
      return false;
   }

   tcg2 = tcg2Local;

   Log(LOG_DEBUG, "TCG2 protocol %u.%u initialized",
       capability.ProtocolVersion.Major,
       capability.ProtocolVersion.Minor);
   return true;
}

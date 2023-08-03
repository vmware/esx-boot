/*******************************************************************************
 * Copyright (c) 2020-2022 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * tpm.c -- TPM implementation for EFI.
 */

#include "efi_private.h"
#include "elf.h"
#include "tpm2_int.h"

#include <limits.h>


typedef struct tpm_event {
   uint32_t pcrIndex;
   const uint8_t *data;
   uint64_t dataSize;
   uint32_t eventType;
   const uint8_t *eventData;
   uint64_t eventDataSize;
} tpm_event_t;

typedef struct system_module {
   const char *name;
   uint32_t pcrIndex;
   uint32_t eventType;
   bool versioned;
   bool measured;
} system_module_t;

#pragma pack(push, 1)
typedef struct vm_version_record {
   uint32_t totalSize;
   uint32_t formatVersion;
   uint64_t flags;
   uint16_t componentNameOffset;
   uint16_t componentNameLen;
   uint16_t productNameOffset;
   uint16_t productnameLen;
   uint16_t productVersionOffset;
   uint16_t productVersionLen;
   uint16_t fileNameOffset;
   uint16_t fileNameLen;
   char stringBuf[0];
} vm_version_record_t;
#pragma pack(pop)


#define UPDATE_SYSTEM_PCR 11
#define CORE_SYSTEM_PCR 12
#define STATIC_DATA_PCR 13
#define VARIABLE_DATA_PCR 14

#define TPM_VMK_EVENT_MOD 2
#define TPM_VMK_EVENT_BOOT_OPT 3
#define TPM_VMK_EVENT_CMD_OPT 4
#define TPM_VMK_EVENT_TAG 6
#define TPM_VMK_EVENT_SIGNER 7
#define TPM_VMK_EVENT_VERSION 8

static system_module_t systemModules [] = {
   { "b",        CORE_SYSTEM_PCR,    TPM_VMK_EVENT_MOD,  true,   false },
   { "k",        CORE_SYSTEM_PCR,    TPM_VMK_EVENT_MOD,  true,   false },
   { "s",        CORE_SYSTEM_PCR,    TPM_VMK_EVENT_MOD,  false,  false },
   { "sb",       CORE_SYSTEM_PCR,    TPM_VMK_EVENT_MOD,  false,  false },
   { "esxupdt",  UPDATE_SYSTEM_PCR,  TPM_VMK_EVENT_MOD,  false,  false },
   { NULL, 0, 0, false, false }
};

static bool useTpm = false;


/*-- tpm_get_event_log --------------------------------------------------------
 *
 *      Return a reference to the EFI memory location of the TPM event
 *      log. We only support the EFI_TCG2_EVENT_LOG_FORMAT_TCG_2 format,
 *      and an error is returned if that format is not available.
 *
 *      See TCG EFI Protocol Specification, Family “2.0”, Level 00
 *      Revision 00.13, March 30, 2016, Section 5: Event Log Structure
 *
 * Parameters
 *      IN log:  The TPM event log details.
 *
 * Results
 *      ERR_SUCCESS, or an error status.
 *----------------------------------------------------------------------------*/
int tpm_get_event_log(tpm_event_log_t *log)
{
   const uint8_t *address;
   uint32_t size;
   bool truncated;
   EFI_STATUS status;

   if (!useTpm) {
      return ERR_NOT_FOUND;
   }

   status = tcg2_get_event_log(&address, &size, &truncated);
   if (status != EFI_SUCCESS) {
      int error = error_efi_to_generic(status);
      Log(LOG_DEBUG, "TPM event log not available: %s", error_str[error]);
      return error;
   }

   log->address = address;
   log->size = size;
   log->truncated = truncated;

   Log(LOG_DEBUG, "TPM event log size: %u", size);
   return ERR_SUCCESS;
}

/*-- tpm_extend_tagged_event ---------------------------------------------------
 *
 *      Extend the TPM with a tagged event. This function will both
 *      extend the event data into the specified TPM PCR and also add
 *      an entry into the event log.
 *
 *      See TCG PC Client Platform Firmware Profile Specification,
 *      Family “2.0”, Level 00 Revision 1.04, June 3, 2019, Section
 *      9.4.2 Tagged Event Log Structure
 *
 * Parameters
 *      IN event: The event to be logged.
 *
 * Results
 *      ERR_SUCCESS, or an error status.
 *----------------------------------------------------------------------------*/
static int tpm_extend_tagged_event(const tpm_event_t *event)
{
   TCG_PCClientTaggedEvent *tEvent;
   uint32_t tEventHeaderSize;
   uint32_t tEventSize;
   uint8_t *tEventData;
   EFI_STATUS status;

   EFI_ASSERT(useTpm);

   tEventHeaderSize = sizeof(tEvent->taggedEventID) +
                      sizeof(tEvent->taggedEventDataSize);
   tEventSize = tEventHeaderSize + event->eventDataSize;
   tEvent = sys_malloc(tEventSize);
   if (tEvent == NULL) {
      return ERR_OUT_OF_RESOURCES;
   }
   tEventData = (uint8_t *)(UINTN)tEvent + tEventHeaderSize;

   tEvent->taggedEventID = event->eventType;
   tEvent->taggedEventDataSize = event->eventDataSize;
   memcpy(tEventData, event->eventData, event->eventDataSize);

   /*
    * The spec referenced above states that "Tagged Event Data MUST be
    * measured and logged using the TCG_PCR_EVENT2 structure". Note that
    * tcg2_log_extend_event only logs when the EFI_TCG2_EVENT_LOG_FORMAT_TCG_2
    * format is in use, but we don't know if the older TCG_1_2 is also in
    * use. That should be OK because we never use the older  log format
    * anyway.
    */

   status = tcg2_log_extend_event(event->pcrIndex, event->data, event->dataSize,
                                  EV_EVENT_TAG, (uint8_t *)tEvent, tEventSize);
   sys_free(tEvent);
   if (status != EFI_SUCCESS) {
      int error = error_efi_to_generic(status);
      Log(LOG_ERR, "TPM log extend failed for ID %u: %s", event->eventType,
          error_str[error]);
      return error;
   }

   return ERR_SUCCESS;
}

/*-- get_module_version_section ------------------------------------------------
 *
 *      Look for a ".version" section in a module.
 *
 *      This function will determine if a module is an ELF binary, and
 *      if so search it for the .version section.
 *
 * Parameters
 *      IN  buffer: Pointer to the binary buffer.
 *      IN  buflen: The size of the binary buffer.
 *      OUT addr:   Pointer to the .version section.
 *      OUT size:   The size of the .version section.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int get_module_version_section(const void *buffer, size_t buflen,
                                      void **addr, size_t *size)
{
   // Need to drop constness for ELF helpers, unfortunately.
   Elf_CommonEhdr *ehdr = (Elf_CommonEhdr *)buffer;
   uint16_t numSections;
   int i;

   // All modules should be 64-bit ELF, and 64-bit is bigger anyway.
   if (buflen < Elf_CommonEhdrSize(TRUE) || !IS_ELF(*ehdr)) {
      return ERR_NOT_FOUND;
   }
   if (buflen > SSIZE_MAX) {
      return ERR_INVALID_PARAMETER;
   }

   numSections = Elf_CommonEhdrGetShNum(ehdr);

   for (i = 0; i < numSections; i++) {
      const Elf_CommonShdr *shdr = Elf_CommonShdrGet(ehdr, i);
      ssize_t shdrEndOff;

      shdrEndOff = (const uint8_t *)shdr + sizeof(Elf_CommonShdr) -
                   (const uint8_t *)buffer;
      if ((const uint8_t *)shdr < (const uint8_t *)buffer ||
          shdrEndOff > (ssize_t)buflen) {
         // Bad ELF
         return ERR_INVALID_PARAMETER;
      }

      if (strcmp(".version", Elf_GetSectionName(ehdr, i)) == 0) {
         uint8_t *secAddr = Elf_CommonShdrGetContents(ehdr, i);
         size_t secSize = Elf_CommonShdrGetSize(ehdr, i);
         ssize_t secEndOff;

         secEndOff = secAddr + secSize - (const uint8_t*)buffer;
         if (secAddr < (const uint8_t *)buffer || secEndOff > (ssize_t)buflen) {
            // Bad ELF
            return ERR_INVALID_PARAMETER;
         }

         *addr = secAddr;
         *size = secSize;
         return ERR_SUCCESS;
      }
   }

   return ERR_NOT_FOUND;
}

/*-- tpm_extend_version --------------------------------------------------------
 *
 *      Extend the TPM with module version information.
 *
 *      Errors are logged, but ignored. A failure to log an event may
 *      mean that an attestation verifier will be unable to determine
 *      the kernel version. That may, in turn, cause remote attestation
 *      to fail.
 *
 * Parameters
 *      IN filename: The file path of the module.
 *      IN moduleAddr: The address of the module.
 *      IN moduleSize: The size of the module in memory.
 *
 * Results
 *      ERR_SUCCESS, or an error status.
 *----------------------------------------------------------------------------*/
static int tpm_extend_version(const char *filename,
                              const void *moduleAddr,
                              size_t moduleSize)
{
   size_t filenameSize = strlen(filename) + 1;
   size_t sectionSize = 0;
   void *sectionAddr = NULL;
   vm_version_record_t *record;
   uint8_t *eventData;
   uint64_t eventDataSize;
   tpm_event_t event;
   int error;

   EFI_ASSERT(useTpm);

   error = get_module_version_section(moduleAddr, moduleSize,
                                      &sectionAddr, &sectionSize);
   if (error != ERR_SUCCESS) {
      // No version information available.
      return error;
   }
   EFI_ASSERT(sectionAddr != NULL);

   record = sectionAddr;
   if (sectionSize < sizeof(*record) || sectionSize < record->totalSize ||
       record->formatVersion == 0 || filenameSize > UINT16_MAX) {
      return ERR_INVALID_PARAMETER;
   }

   /*
    * The ".version" section may contain multiple records, but we
    * currently only expect one. Take only the first record if there
    * is more than one.
    */
   eventDataSize = record->totalSize + filenameSize;
   if (eventDataSize > UINT32_MAX) {
      return ERR_INVALID_PARAMETER;
   }
   eventData = sys_malloc(eventDataSize);
   if (eventData == NULL) {
      return ERR_OUT_OF_RESOURCES;
   }

   event.pcrIndex = STATIC_DATA_PCR;
   event.data = eventData;
   event.dataSize = eventDataSize;
   event.eventType = TPM_VMK_EVENT_VERSION;
   event.eventData = eventData;
   event.eventDataSize = eventDataSize;

   /*
    * We don't validate the record offsets and lengths. These will need
    * to be validated by anyone parsing the event log.
    */
   memcpy(eventData, record, record->totalSize);
   memcpy(eventData + record->totalSize, filename, filenameSize);

   // Fixup the record sizes and offsets based on adding the filename.
   record = (vm_version_record_t *)eventData;
   record->fileNameOffset = record->totalSize;
   record->fileNameLen = (uint16_t)filenameSize;
   record->totalSize = eventDataSize;

   error = tpm_extend_tagged_event(&event);

   sys_free(eventData);
   return error;
}

/*-- tpm_extend_module ---------------------------------------------------------
 *
 *      Extend the TPM with a loaded module.
 *
 * Parameters
 *      IN filename: The name of the module.
 *      IN addr: The address of the module.
 *      IN size: The size of the module in memory.
 *
 * Results
 *      ERR_SUCCESS, or an error status.
 *----------------------------------------------------------------------------*/
int tpm_extend_module(const char *filename,
                      const void *addr,
                      size_t size)
{
   static const uint8_t zeroByte = 0;

   system_module_t *module;
   const char *basename;
   const char *ext;
   size_t normalizedLen;
   uint32_t pcrIndex;
   uint32_t eventType;
   tpm_event_t event;

   if (!useTpm) {
      return ERR_SUCCESS;
   }

   basename = strrchr(filename, '/');
   if (basename == NULL) {
      basename = filename;
   } else {
      basename++;
   }

   ext = strrchr(basename, '.');
   if (ext == NULL) {
      normalizedLen = strlen(basename);
   } else {
      normalizedLen = ext - basename;
      ext++;
   }

   if (size == 0) {
      /*
       * The addr will be zero when size is zero. But the UEFI runtime
       * will return an error when addr is zero. Give a valid addr to
       * make the UEFI runtime happy.
       */
      addr = &zeroByte;

      /*
       * Specifying a zero-length buffer should work, but we have seen
       * some firmware implementations that are unable to handle it and
       * instead produce a corrupt event log. Use a single zero-byte
       * instead. See PR 3018228.
       */
      size = sizeof(zeroByte);
   }

   /*
    * Rules for module measurement:
    *
    * 1. There are a set of modules that form the base of the kernel and
    *    must be measured into CORE_SYSTEM_PCR or UPDATE_SYSTEM_PCR
    *    with an event type TPM_VMK_EVENT_MOD.
    *
    * 2. Any module that ends with a ".gz" extension must be measured
    *    into VARIABLE_DATA_PCR with event type TPM_VMK_EVENT_BOOT_OPT.
    *
    * 3. All other modules are measured in STATIC_DATA_PCR with event
    *    type TPM_VMK_EVENT_MOD.
    */

   module = systemModules;
   while (module->name != NULL) {
      if (strncmp(basename, module->name, normalizedLen) == 0 &&
          module->name[normalizedLen] == '\0') {
         /*
          * We always measure everything into the TPM. If we have a
          * duplicate here, it may result in an unseal failure.
          */
         if (module->measured) {
            Log(LOG_WARNING, "Duplicate modules named %s", module->name);
         }

         if (module->versioned) {
            int error = tpm_extend_version(filename, addr, size);
            if (error != ERR_SUCCESS && error != ERR_NOT_FOUND) {
               Log(LOG_WARNING, "Failed to measure version for %s: %s",
                   filename, error_str[error]);
            }
         }

         pcrIndex = module->pcrIndex;
         eventType = module->eventType;

         module->measured = true;
         goto done;
      }
      module++;
   }

   if (ext != NULL && strcmp(ext, "gz") == 0) {
      pcrIndex = VARIABLE_DATA_PCR;
      eventType = TPM_VMK_EVENT_BOOT_OPT;
   } else {
      pcrIndex = STATIC_DATA_PCR;
      eventType = TPM_VMK_EVENT_MOD;
   }

 done:
   event.pcrIndex = pcrIndex;
   event.data = (uint8_t *)addr;
   event.dataSize = (uint32_t)size;
   event.eventType = eventType;
   event.eventData = (uint8_t *)basename;
   event.eventDataSize = strlen(basename) + 1;

   return tpm_extend_tagged_event(&event);
}

/*-- tpm_extend_signer ---------------------------------------------------------
 *
 *      Extend the TPM with a certificate or public key. The data should
 *      be in DER format.
 *
 * Parameters
 *      IN certData: The certificate data.
 *      IN certLength: The size of the certificate data in memory.
 *
 * Results
 *      ERR_SUCCESS, or an error status.
 *----------------------------------------------------------------------------*/
int tpm_extend_signer(const unsigned char *certData,
                      uint16_t certLength)
{
   tpm_event_t event;

   if (!useTpm) {
      return ERR_SUCCESS;
   }

   event.pcrIndex = VARIABLE_DATA_PCR;
   event.data = (uint8_t *)certData;
   event.dataSize = (uint32_t)certLength;
   event.eventType = TPM_VMK_EVENT_SIGNER;
   event.eventData = (uint8_t *)certData;
   event.eventDataSize = certLength;

   return tpm_extend_tagged_event(&event);
}

/*-- tpm_extend_cmdline --------------------------------------------------------
 *
 *      Extend the TPM with the kernel command line.
 *
 * Parameters
 *      IN filename: The kernel file name.
 *      IN options: The kernel command line options.
 *
 * Results
 *      ERR_SUCCESS, or an error status.
 *----------------------------------------------------------------------------*/
int tpm_extend_cmdline(const char *filename,
                       const char *options)
{
   tpm_event_t event;
   char *cmdline;
   int len;
   int result;

   if (!useTpm) {
      return ERR_SUCCESS;
   }

   if (options == NULL || options[0] == '\0') {
      len = asprintf(&cmdline, "%s", filename);
   } else {
      len = asprintf(&cmdline, "%s %s", filename, options);
   }

   if (len == -1) {
      return ERR_OUT_OF_RESOURCES;
   }

   /*
    * Note that the event data does not include the terminating null-
    * character. While it would be better to include it (to make
    * printing easy), we need to maintain backward compatibility.
    */

   event.pcrIndex = VARIABLE_DATA_PCR;
   event.data = (uint8_t *)cmdline;
   event.dataSize = strlen(cmdline);
   event.eventType = TPM_VMK_EVENT_CMD_OPT;
   event.eventData = (uint8_t *)cmdline;
   event.eventDataSize = strlen(cmdline);

   result = tpm_extend_tagged_event(&event);

   sys_free(cmdline);

   return result;
}

/*-- tpm_extend_asset_tag ------------------------------------------------------
 *
 *      Extend the TPM with the asset tag NV value.
 *
 * Results
 *      ERR_SUCCESS, or an error status.
 *----------------------------------------------------------------------------*/
int tpm_extend_asset_tag(void)
{
   const uint32_t TPM2_TAG_INDEX = 0x01C10110;
   tpm_event_t event;
   uint8_t tag[512];
   uint16_t tagSize;
   EFI_STATUS status;

   if (!useTpm) {
      return ERR_SUCCESS;
   }

   status = tpm2_nv_read_size(TPM2_TAG_INDEX, &tagSize);
   if (status == EFI_NOT_FOUND) {
      // The common case is that the NV asset tag is not set.
      return ERR_SUCCESS;
   }
   if (status != EFI_SUCCESS) {
      int error = error_efi_to_generic(status);
      Log(LOG_ERR, "Failed to determine TPM asset tag size: %s",
          error_str[error]);
      return error;
   }

   if (tagSize > sizeof tag) {
      Log(LOG_ERR, "TPM asset tag too large: %u bytes", tagSize);
      return ERR_BUFFER_TOO_SMALL;
   }

   status = tpm2_nv_read(TPM2_TAG_INDEX, tagSize, (uint8_t *)&tag);
   if (status == EFI_NOT_FOUND || status == EFI_NOT_READY) {
      // NOT_READY could happen if index was defined but not written.
      return ERR_SUCCESS;
   }
   if (status != EFI_SUCCESS) {
      int error = error_efi_to_generic(status);
      Log(LOG_ERR, "Failed to read TPM asset tag: %s", error_str[error]);
      return error;
   }

   event.pcrIndex = VARIABLE_DATA_PCR;
   event.data = tag;
   event.dataSize = tagSize;
   event.eventType = TPM_VMK_EVENT_TAG;
   event.eventData = tag;
   event.eventDataSize = tagSize;

   return tpm_extend_tagged_event(&event);
}

/*-- tpm_init ------------------------------------------------------------------
 *
 *      Initialize TPM services.
 *
 * Parameters
 *      None
 *
 * Results
 *      None
 *----------------------------------------------------------------------------*/
void tpm_init(void)
{
   useTpm = tcg2_init();
}

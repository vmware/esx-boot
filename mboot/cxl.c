/*******************************************************************************
 * Copyright (c) 2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc.
 * and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * cxl.c -- Compute Express Link(CXL) protocol related code.
 */
#include <error.h>
#include <acpi_common.h>
#include <bootlib.h>
#include <boot_services.h>

#define FOREACH_ACPI_CEDT_STRUCTURE(table, header)                         \
   do {                                                                    \
   for ((header) = (acpi_cedt_struct_header *) (table)->structs;           \
        (char *)(header) < (char *)(table) + (table)->header.length;       \
        (header) = (acpi_cedt_struct_header *)((char *)(header) +          \
                    (header)->length))                                     \

#define FOREACH_ACPI_CEDT_STRUCTURE_DONE                                   \
   } while(0);

#define CXL_MAX_NUM_INT_WAYS (12)
#define CXL_INVALID_INT_WAYS 0

typedef struct chbs_info_t {
   uint32_t uid;
} chbs_info_t;

typedef struct cfmws_info_t {
   uint64_t baseHPA;
   uint64_t windowSize;
   uint8_t numInterleaveWays;
   uint32_t targetList[CXL_MAX_NUM_INT_WAYS];
   uint32_t numCxlTargets;
} cfmws_info_t;

static chbs_info_t *chbsInfos;
static cfmws_info_t *cfmwsInfos;
static uint32_t numCHBS = 0;
static uint32_t numCFMWS = 0;

/*-- decode_interleave_ways ---------------------------------------------
 *
 *      Decode encoded interleave ways present in the CEDT table to get the
 *      number of interleave ways.
 *
 * Parameters
 *      IN intWaysEncode:  encoded intereleave ways number found in CFMWS entry.
 *
 * Results
 *      decoded number of interleave ways
 *----------------------------------------------------------------------------*/

static uint32_t decode_interleave_ways(uint8_t intWaysEncode)
{
   uint32_t intWays = CXL_INVALID_INT_WAYS;

   switch (intWaysEncode) {
   case 0 ... 4:
      intWays = 1 << intWaysEncode;
      break;
   case 8 ... 10:
      intWays = 3 << (intWaysEncode - 8);
      break;
   default:
      Log(LOG_WARNING, "Unexpected value when decoding interleave ways");
   }

   return intWays;
}

/*-- parse_acpi_cedt ---------------------------------------------------------------
 *
 *      Parse and store relevant info of CEDT(CXL Early Discovery Table).
 *
 * Results
 *      ERR_SUCCESS if sucessfully parsed or appropriate error code
 *----------------------------------------------------------------------------*/

int parse_acpi_cedt(void)
{
   acpi_cedt_table *table;
   acpi_cedt_struct_header *header;
   uint32_t tempNumCFMWS = 0, tempNumCHBS = 0;
   int status = ERR_SUCCESS;

   table = (void *) acpi_find_sdt("CEDT");
   if (table == NULL) {
      Log(LOG_DEBUG, "No ACPI CEDT table found");
      return ERR_SUCCESS;
   }

   FOREACH_ACPI_CEDT_STRUCTURE(table, header) {
      if (header->type == ACPI_CEDT_STRUCT_TYPE_CHBS) {
         tempNumCHBS++;
      }
      if (header->type == ACPI_CEDT_STRUCT_TYPE_CFMWS) {
         tempNumCFMWS++;
      }
   } FOREACH_ACPI_CEDT_STRUCTURE_DONE;

   chbsInfos = malloc(sizeof(*chbsInfos) * tempNumCHBS);
   if (chbsInfos == NULL) {
      return ERR_OUT_OF_RESOURCES;
   }
   memset(chbsInfos, 0, sizeof(*chbsInfos) * tempNumCHBS);

   cfmwsInfos = malloc(sizeof(*cfmwsInfos) * tempNumCFMWS);
   if (chbsInfos == NULL) {
      free(chbsInfos);
      return ERR_OUT_OF_RESOURCES;
   }
   memset(cfmwsInfos, 0, sizeof(*cfmwsInfos) * tempNumCFMWS);

   FOREACH_ACPI_CEDT_STRUCTURE(table, header) {
      if (header->type == ACPI_CEDT_STRUCT_TYPE_CHBS) {
         acpi_cedt_chbs_struct *chbs = (acpi_cedt_chbs_struct *) header;

         chbsInfos[numCHBS].uid = chbs->uid;
         numCHBS++;
      }
      if (header->type == ACPI_CEDT_STRUCT_TYPE_CFMWS) {
         uint32_t numInterleaveWays;
         acpi_cedt_cfmws_struct *cfmws = (acpi_cedt_cfmws_struct *) header;
         cfmws_info_t *cfmws_info = &cfmwsInfos[numCFMWS];

         cfmws_info->baseHPA = cfmws->baseHPA;
         cfmws_info->windowSize = cfmws->windowSize;
         numInterleaveWays = decode_interleave_ways(cfmws->interleaveWays);
         if (numInterleaveWays == CXL_INVALID_INT_WAYS ||
             numInterleaveWays > CXL_MAX_NUM_INT_WAYS) {
            Log(LOG_ERR, "Unexpected number of interleave ways: %u",
                numInterleaveWays);
            status = ERR_UNSUPPORTED;
            goto bail;
         }
         cfmws_info->numInterleaveWays = numInterleaveWays;
         memcpy(cfmws_info->targetList, cfmws->targetList,
                  sizeof(uint32_t) * numInterleaveWays);
         numCFMWS++;
      }
   } FOREACH_ACPI_CEDT_STRUCTURE_DONE;

   return status;

bail:
   numCFMWS = 0;
   numCHBS = 0;
   free(chbsInfos);
   free(cfmwsInfos);
   return status;
}

/*-- is_interleave_target_cxl -------------------------------------------------
 *
 *      Check if the given uid corresponds to a CXL root bridge uid.
 *
 * Parameters
 *      IN uid:  uid to check
 *
 * Results
 *      true/false
 *----------------------------------------------------------------------------*/

static bool is_interleave_target_cxl(uint32_t uid)
{
   uint32_t i;

   for (i = 0; i < numCHBS; i++) {
      if (chbsInfos[i].uid == uid) {
         return true;
      }
   }
   return false;
}

/*-- system_has_hetero_interleaving --------------------------------------------
 *
 *      Check if system is in CXL hetergenous mode based on the information
 *      present in CEDT table. System is in hetegenous interleaving mode if
 *      any of CFMWS memory window has a non CXL root bridge uid in the
 *      respective interleave target list.
 *
 * Results
 *      true/false
 *----------------------------------------------------------------------------*/

static bool system_has_hetero_interleaving(void)
{
   uint32_t i,j;
   cfmws_info_t *cfmws;

   for (i = 0; i < numCFMWS; i++) {

      cfmws = &cfmwsInfos[i];
      cfmws->numCxlTargets = 0;
      for (j = 0; j < cfmws->numInterleaveWays; j++) {
         if (is_interleave_target_cxl(cfmws->targetList[j])) {
            cfmws->numCxlTargets++;
         }
      }
      if (cfmws->numCxlTargets != cfmws->numInterleaveWays) {
         return true;
      }
   }
   return false;
}

/*-- blacklist_cxl_memory ------------------------------------------------------
 *
 *    VMware decided not to use CXL memory during early boot for the following
 *    reasons based on the type of CXL memory:
 *       1. CXL Type 2:
 *          As the CXL spec defines it this memory is suposed to be used for
 *          accelerating special purpose that the device is designed for.
 *          Firmware will always tag this memory as SPM(Specific Purpose Memory)
 *          with EFI_SP UEFI memory attribute in the memory map as a hint to the
 *          boot loader/OS not to use this memory for general purpose
 *          allocations.
 *       2. CXL Type 3:
 *          We decided not to use this memory until we can make clear
 *          determination about the usability of the device based on some policy
 *          setting. This evaluation is to be done in vmkernel, so better we
 *          avoid using CXL memory.
 *
 *    Note: When system is in heterogenous interleaving mode(that is
 *    CXL type 3 memory interleaved with DRAM), we are not doing any
 *    blacklisting.
 *
 *    Our boot loader uses memory based on the boot stage as follows:
 *       1. Before UEFI ExitBootServices:
 *             During this phase, we rely on UEFI boot services for all
 *             memory allocations and hence have to trust that firmware is not
 *             going to allocate CXL type 2 or type 3 memory when requested for
 *             memory.
 *       2. Afer ExitBootServices:
 *             Boot loader will relocate boot modules to the memory that is
 *             marked as available in memory map, the relocation phase.
 *             This function makes sure that we don't relocate any of our
 *             boot payloads into CXL memory despite that being marked as
 *             available in UEFI memory map.
 *
 *    Parse the ACPI CEDT(CXL Early Discovery Table) and blacklist all of
 *    HPA(Host Physical Address) window ranges listed in CFMWS(CXL Fixed Memory
 *    Window Structure) windows.
 *
 *    If all firmware guaranteed that any type of CXL memory will have
 *    EFI_SP (Specific Purpose Memory) memory attribute, there would have been
 *    no use for this function, since we already blacklist all specific purpose
 *    memory. But some platforms might not tag CXL type 3 memory with EFI_SP
 *    UEFI memory attribute as we know now, so this extra step is needed to
 *    achieve our goal of not using CXL memory in early boot.
 *
 * Results
 *    ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int blacklist_cxl_memory(void)
{
   int status;
   uint32_t i;

   if (numCFMWS == 0) {
      return ERR_SUCCESS;
   }

  if (system_has_hetero_interleaving()) {
      Log(LOG_DEBUG, "CXL: System in heterogenous interleave mode, "
                     "CXL memory not blacklisted");
      return ERR_SUCCESS;
   }

   for (i = 0; i < numCFMWS; i++) {
      cfmws_info_t *cfmws = &cfmwsInfos[i];
      /*
       * There could be holes in (baseHPA, baseHPA + windowSize -1), but
       * blacklisting a hole should be harmless.
       *
       * In certain cases (for example CXL type 2), the range could have
       * already been blacklisted when we blacklist
       * SPM(Specifc Purpose Memory) ranges, but blacklisting more than once
       * should be harmless.
       */

      status = blacklist_runtime_mem(cfmws->baseHPA, cfmws->windowSize);
      Log(LOG_DEBUG, "CXL: blacklisting CXL mem range "
                    "(0x%"PRIx64" - 0x%"PRIx64")", cfmws->baseHPA,
                     cfmws->baseHPA + cfmws->windowSize - 1);
      if (status != ERR_SUCCESS) {
         Log(LOG_ERR, "CXL: failed blacklisting CXL mem range "
                      "(0x%"PRIx64" - 0x%"PRIx64")", cfmws->baseHPA,
                      cfmws->baseHPA + cfmws->windowSize - 1);
         return status;
      }
   }

   return ERR_SUCCESS;
}

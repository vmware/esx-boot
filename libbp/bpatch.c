/*******************************************************************************
 * Copyright (c) 2022 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * bpatch.c --
 *
 *      BPATCH main methods for implementing binary patching library. Goal of
 *      library is to extract binary patches information from dedicated ELF
 *      sections and use those informations for applying it. Patch application
 *      consists in modifying a branch opcode with a nop.
 */


#include <stdint.h>
#include <error.h>
#include <bootlib.h>
#include <compat.h>
#include "bpatch.h"

/*
 * Variables used for implementation.
 */
static void *startAddrSecPatchLoc = 0;
static uint32_t secPatchLocSize = 0;
uint64_t bp_offset = 0;

/*
 * Defines used for implementation.
 */
#define FUNC_PREAMBLE_SIZE (4 * sizeof (uint32_t))

/* Size of ACPI OEM table Id */
#define BPATCH_OEM_TABLE_ID_SIZE 8
/* Size of ACPI OEM Id */
#define BPATCH_OEM_ID_SIZE 6

extern int bpatch_apply_func_patch(BinaryPatch *patchElement);
extern void bpatch_modify_opcode(void *patchAddr, uint32_t opcode);
extern int bpatch_reg_system_detect(BPMatchPatchDesc_t *matchPatchGroupElem,
                                    match_type_t matchType);

/*-- bpatch_set_patchloc -----------------------------------------------
 *
 *      This method is in charge of setting values for patch Location
 *      Elf section address and size.
 *
 * Parameters
 *      IN sectionAddr: elf patch loc section address.
 *      IN sectionSize: elf patch loc section size.
 *--------------------------------------------------------------------*/
void bpatch_set_patchloc(void *sectionAddr, uint32_t sectionSize)
{
   if ((sectionAddr != 0) && (sectionSize != 0)) {
      startAddrSecPatchLoc = sectionAddr;
      secPatchLocSize = sectionSize;
   }
}

/*-- bpatch_set_offset -------------------------------------------------
 *
 *      This method is used to set an offset regarding code.
 *
 * Parameters
 *      IN off: address offset to use during patch application.
 *--------------------------------------------------------------------*/
void bpatch_set_offset(uint64_t off)
{
   bp_offset = off;
}

/*-- bpatch_get_patchloc -----------------------------------------------
 *
 *      This method is in charge of getting values for elf patch
 *      Location address and size.
 *
 * Parameters
 *      OUT sectionAddr: elf patch loc section address.
 *      OUT sectionSize: elf patch loc section size.
 *--------------------------------------------------------------------*/
static int bpatch_get_patchloc(void **sectionAddr, uint32_t *sectionSize)
{
   *sectionAddr = startAddrSecPatchLoc;
   *sectionSize = secPatchLocSize;
   return ERR_SUCCESS;
}

/*-- bpatch_get_acpi_tblid ---------------------------------------------
 *
 *      This method is in charge of getting oemTableId stored into
 *      ACPI table.
 *
 * Parameters
 *      IN acpiTableSig: acpi table signature.
 *      OUT oemId: oemId corresponding to signature.
 *      OUT oemTableId: oem Table id stirng correponding to signature.
 *--------------------------------------------------------------------*/
static int bpatch_get_acpi_tblid(uint32_t *acpiTableSig,
                                 char oemId[static BPATCH_OEM_ID_SIZE],
                                 char oemTableId[static BPATCH_OEM_TABLE_ID_SIZE])
{
   int status = ERR_NOT_FOUND;
   acpi_sdt *tblPtr = acpi_find_sdt((const char *)acpiTableSig);

   if (tblPtr != NULL) {
      memcpy(oemId, tblPtr->oem_id, BPATCH_OEM_ID_SIZE);
      memcpy(oemTableId, tblPtr->table_id, BPATCH_OEM_TABLE_ID_SIZE);
      status = ERR_SUCCESS;
   }

   return status;
}

/*-- bpatch_acpi_system_detect -----------------------------------------
 *
 *      Parses the ACPI OEM and product IDs to figure out system it is
 *      running on.
 *
 * Parameters
 *      IN matchPatchGroupElem: pointer to matching group element
 *                              structure.
 *--------------------------------------------------------------------*/
static int bpatch_acpi_system_detect(BPMatchPatchDesc_t *matchPatchGroupElem)
{
   int status;
   char oemId[BPATCH_OEM_ID_SIZE];
   char oemTableId[BPATCH_OEM_TABLE_ID_SIZE];

   if (matchPatchGroupElem == NULL) {
      return ERR_INVALID_PARAMETER;
   }

   status = bpatch_get_acpi_tblid((uint32_t *)
                                  matchPatchGroupElem->matchPatchGroupType.\
                                  acpiTableSig,
                                  oemId,
                                  oemTableId);
   Log(LOG_DEBUG, "AcpiTableSig %s status %d.",
       matchPatchGroupElem->matchPatchGroupType.acpiTableSig, status);
   if (status == ERR_SUCCESS) {
      if ((!strncmp(oemId,
                    matchPatchGroupElem->acpiProcess.oemId,
                    matchPatchGroupElem->acpiProcess.sizeOemId)) &&
          (!strncmp(oemTableId,
                    matchPatchGroupElem->acpiProcess.oemTableId,
                    matchPatchGroupElem->acpiProcess.sizeOemTblId))) {
         status = ERR_SUCCESS;
      } else {
         status = ERR_NOT_FOUND;
      }
   }
   return status;
}

/*-- bpatch_get_patch_grpid --------------------------------------------
 *      Analyse the patch description provided as parameter and return
 *      the patch group Id.
 *
 * Parameters
 *      IN patchDescElement: pointer on patch element description
 *                           structure.
 *      OUT patchGroupId : patch group Id value associated to current
 *                         patch.
 *--------------------------------------------------------------------*/
int bpatch_get_patch_grpid(BPMatchPatchDesc_t *patchDescElement,
                           patch_group_t *patchGroupId)
{
   int status;
   match_type_t matchType;

   if (patchDescElement == NULL || patchGroupId == NULL) {
      return ERR_INVALID_PARAMETER;
   }

   matchType = patchDescElement->matchPatchGroupType;
   Log(LOG_DEBUG, "Found Entry type %x GroupId %x", matchType.type,
       patchDescElement->patchGroup.patchGroupValue);
   patchGroupId->patchGroupValue = patchDescElement->patchGroup.patchGroupValue;
   switch (matchType.type) {
   case MATCH_ARM_SYS_REG:
      status = bpatch_reg_system_detect(patchDescElement, matchType);
      break;
   case MATCH_OEM_ID_ACPI:
      status = bpatch_acpi_system_detect(patchDescElement);
      break;
   default:
      status = ERR_INVALID_PARAMETER;
      break;
   }

   if (status == ERR_SUCCESS) {
      patchGroupId->patchGroupValue =
         patchDescElement->patchGroup.patchGroupValue;
   }
   return status;
}


/*-- bpatch_find_placeholder -------------------------------------------
 *
 *      In the section created by the compiler, let's look for an
 *      address, at the beginning of the function to patch.
 *
 * Parameters
 *      IN srcAddr: function pointer to patch.
 *      OUT patchAddr: placeholder address.
 *--------------------------------------------------------------------*/
int bpatch_find_placeholder(void *srcAddr, void **patchAddr)
{
   void *startSection;
   uint32_t sizeSection = 0;
   int status;

   status = bpatch_get_patchloc(&startSection,
                                &sizeSection);
   if (status == ERR_SUCCESS) {
      uint32_t index;
      uint64_t *ptr = (uint64_t *)startSection;
      uint32_t nbElement = sizeSection / sizeof(uint64_t);

      Log(LOG_DEBUG, "Detect patchLoc section start address and size: %p %d",
          startSection, sizeSection);
      status = ERR_NOT_FOUND;
      for (index = 0; index < nbElement; index ++) {
         if (ptr[index] - (intptr_t)srcAddr < FUNC_PREAMBLE_SIZE) {
            Log(LOG_DEBUG, "Detect an entry for your @%"PRIx64 " index is %d",
                ptr[index],
                index);
            *((uint64_t *)patchAddr) = ptr[index];
            return ERR_SUCCESS;
         }
      }
   }

   Log(LOG_DEBUG, "Required placeholder not found for function at %p",
       srcAddr);

   return status;
}

/*-- bpatch_apply_data_patch -------------------------------------------
 *
 *      Modify the value of the variable to patch.
 *
 * Parameters
 *      IN patchElement : pointer to patch element structure.
 *--------------------------------------------------------------------*/
static int bpatch_apply_data_patch(BinaryPatch *patchElement)
{
   void *srcAddr = (void *) ((uint64_t)patchElement->functionToPatchAddr +
                   bp_offset);

   if (srcAddr == 0 || patchElement->writeSize == 0 ||
       patchElement->writeSize > sizeof(uint64_t)) {
      return ERR_INVALID_PARAMETER;
   }

   Log(LOG_DEBUG, "Modifying variable at %p - newValue 0x%"PRIx64" size %d",
       srcAddr, patchElement->newValue, patchElement->writeSize);

   memcpy(srcAddr, (void *)(&(patchElement->newValue)),
          patchElement->writeSize);

   patchElement->patchLocationAddr = srcAddr;
   patchElement->isApplied = TRUE;
   return ERR_SUCCESS;
}

/*-- bpatch_apply_zone_patch -------------------------------------------
 *
 *      Replace the first opcodes of patched zone with opcode provided
 *      with patch description structure.
 *
 * Parameters
 *      IN patchElement : pointer to patch element structure.
 *--------------------------------------------------------------------*/
static int bpatch_apply_zone_patch(BinaryPatch *patchElement)
{
   uint32_t opcode = patchElement->newOpcode;
   void* patchAddr = patchElement->functionToPatchAddr;

   if ((patchAddr == 0) || (patchElement->writeSize != sizeof opcode)) {
      return ERR_INVALID_PARAMETER;
   }

   Log(LOG_DEBUG, "Let's patch a Zone at 0x%p size = 0x%x", patchAddr,
       patchElement->writeSize);

   bpatch_modify_opcode(patchAddr, opcode);
   patchElement->patchLocationAddr = patchAddr;
   patchElement->isApplied = TRUE;

   return ERR_SUCCESS;
}

/*-- bpatch_apply_patch ------------------------------------------------
 *
 *      Generic function to apply a patch. It is using the patch type to
 *      select the correct method for applying a patch.
 *
 *      Let's assume the patch is applied when the elf module is loaded
 *      which means it is assumed the memory is writable and that after
 *      patch is applied the data cache will be clean and instruction
 *      cache invalidated.
 *
 * Parameters:
 *      IN patchElement: pointer to patch element structure.
 *      IN patchGroupId: patch group id value.
 *--------------------------------------------------------------------*/
int bpatch_apply_patch(BinaryPatch *patchElement, uint32_t patchGroupId)
{
   int status;
   PatchType type;

   if (patchElement == NULL) {
      return ERR_INVALID_PARAMETER;
   }
   if (patchElement->patchGroupId != patchGroupId) {
      /* Nothing to do. */
      return ERR_ABORTED;
   }
   if (patchElement->isApplied == TRUE) {
      /* Patch is already applied. */
      return ERR_SUCCESS;
   }

   type = patchElement->type;
   switch (type) {
   case FUNCTION_PATCH:
      status = bpatch_apply_func_patch(patchElement);
      break;
   case ZONE_PATCH:
      status = bpatch_apply_zone_patch(patchElement);
      break;
   case DATA_PATCH:
      status = bpatch_apply_data_patch(patchElement);
      break;
   default:
      Log(LOG_ERR, "Patch Type is not supported %d", type);
      status = ERR_UNSUPPORTED;
      break;
   }
   return status;
}

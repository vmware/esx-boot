/*******************************************************************************
 * Copyright (c) 2022-2023 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * bp_apply.c --
 *
 *      Binary Patching related functionality.
 */

#include <stdint.h>
#include <bootlib.h>
#include <elf.h>
#include "bpatch.h"

static void *imagePatchLocBaseAddr = 0;
static Elf64_Size imagePatchLocLength = 0;
static void *imagePatchArrayBaseAddr = 0;
static Elf64_Size imagePatchArrayLength = 0;
static void *imageMatchPatchGroupBaseAddr = 0;
static Elf64_Size imageMatchPatchGroupLength = 0;

#define ELF_SECTION_NAME_MAX_SIZE 32
#define STR_PATCHLOC_SEC "__patchable_function_entries"
#define STR_PATCH_ARRAY_SEC ".bpatch_array"
#define STR_MATCH_PATCH_GROUP_SEC ".match_patch_group"

/*-- bapply_elf_get_sec_addr -------------------------------------------
 *
 *      Given an elf file get a section's starting address and add the
 *      randomOffset.
 *
 * Parameters
 *      IN ehdr: pointer to elf binary.
 *      IN section: elf section index.
 *--------------------------------------------------------------------*/
static INLINE void * bapply_elf_get_sec_addr(void *ehdr, int section)
{
   return  (void *)((char *) ehdr + Elf_CommonShdrGetOff(ehdr, section));
}

/*-- bapply_elf_collect_info -------------------------------------------
 *
 *      Parse the Elf section and save section start address and
 *      size. It will be used during binary patching operation.
 *
 * Parameters
 *      IN ehdr: pointer to elf binary.
 *--------------------------------------------------------------------*/
static void bapply_elf_collect_info(void *ehdr)
{
   Elf64_Quarter i;
   Elf64_Quarter numSections = Elf_CommonEhdrGetShNum(ehdr);
   char *names =
      Elf_CommonShdrGetContents(ehdr, Elf_CommonEhdrGetShStrNdx(ehdr));

   for (i = 0; i < numSections; i++) {
      char name[ELF_SECTION_NAME_MAX_SIZE];
      void *va;
      Elf64_Size len;

      if (Elf_CommonShdrGetType(ehdr, i) == SHT_NULL) {
         continue;
      }

      strcpy(name, &names[Elf_CommonShdrGetName(ehdr, i)]);
      va = bapply_elf_get_sec_addr(ehdr, i);
      len = Elf_CommonShdrGetSize(ehdr, i);

      if (strcmp(name, STR_PATCHLOC_SEC) == 0) {
         imagePatchLocBaseAddr = va;
         imagePatchLocLength   = len;
      } else if (strcmp(name, STR_PATCH_ARRAY_SEC) == 0) {
         imagePatchArrayBaseAddr = va;
         imagePatchArrayLength   = len;
      } else if (strcmp(name, STR_MATCH_PATCH_GROUP_SEC) == 0) {
         imageMatchPatchGroupBaseAddr = va;
         imageMatchPatchGroupLength   = len;
         bpatch_set_offset((uint64_t)((uint64_t)va -
                           Elf_CommonShdrGetAddr(ehdr, i)));
      }
   }
}

/*-- bapply_patch_esxinfo ----------------------------------------------
 *
 *      This function is in charge of applying if needed kernel binary
 *      patches.
 *      patched memory must be writable and after this call data and
 *      instruction caches will be maintained by caller.
 *
 *      Binary patching operation is splitted into multiple parts:
 *       1- identify ELF sections used for patching.
 *       2- parse patch matching section to detect if a group of patch
 *          has to be applied.
 *       3- parse patch element section and for each element part of the
 *          selected group let's apply it.
 *
 * Parameters
 *      IN ehdr: pointer to elf binary.
 *--------------------------------------------------------------------*/
int bapply_patch_esxinfo(void *ehdr)
{
   int status = ERR_NOT_FOUND;
   uint32_t index;
   uint32_t nbAvailablePatch;
   Elf64_Size matchSecSize;
   BinaryPatch *patchElementPtr;
   BPMatchPatchDesc_t *matchSecPtr;
   patch_group_t patchGroupId;

   if (ehdr == NULL) {
      Log(LOG_ERR, "Bad Elf pointer");
      return ERR_INVALID_PARAMETER;
   }

   Log(LOG_DEBUG, "Applying Binary Patches.");

   /* Let's process the binary and get Elf section we are looking for. */
   bapply_elf_collect_info(ehdr);

   if (imageMatchPatchGroupBaseAddr == 0 ||
      imageMatchPatchGroupLength == 0) {
      Log(LOG_DEBUG, "No Matching Patch Group section found.");
      return ERR_SUCCESS;
   }

   /* Calculate the number of patch we have. */
   nbAvailablePatch = (uint32_t)(imagePatchArrayLength / sizeof(BinaryPatch));
   Log(LOG_DEBUG, "%d Binary patches embedded into image.", nbAvailablePatch);
   if (nbAvailablePatch == 0 ||
      imagePatchArrayBaseAddr == 0) {
      return ERR_SUCCESS;
   }

   /* Set the Patch Location Section for future. */
   bpatch_set_patchloc(imagePatchLocBaseAddr, imagePatchLocLength);

   /*
    * Let's parse the matching patch section and if we detect a groupId that
    * needs to be installed let's parse the patch element section to apply
    * patches part of the selected groupId.
    */
   matchSecSize = imageMatchPatchGroupLength;
   matchSecPtr = (BPMatchPatchDesc_t *)imageMatchPatchGroupBaseAddr;
   while (matchSecSize >= sizeof(BPMatchPatchDesc_t)) {
      status = bpatch_get_patch_grpid(matchSecPtr, &patchGroupId);
      if (status == ERR_SUCCESS) {
         Log(LOG_DEBUG, "Applying patchGroupId %d.",
             patchGroupId.patchGroupValue);
         patchElementPtr = (BinaryPatch *)imagePatchArrayBaseAddr;
         for (index = 0; index < nbAvailablePatch; index++) {
            status = bpatch_apply_patch(patchElementPtr,
                                        patchGroupId.patchGroupValue);
            if (status == ERR_SUCCESS) {
               Log(LOG_DEBUG, "Patch  %d successfully applied.", index);
            }
            patchElementPtr++;
         }
      } else if (status == ERR_NOT_FOUND) {
         Log(LOG_DEBUG, "patchGroupId %d not applied",
             patchGroupId.patchGroupValue);
      } else {
         Log(LOG_ERR, "Error getting a new patchGroupId .");
      }
      matchSecSize -= sizeof(BPMatchPatchDesc_t);
      matchSecPtr++;
   }
   return status;
}

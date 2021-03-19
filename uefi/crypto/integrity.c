/*******************************************************************************
 * Copyright (c) 2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 *  integrity.c --
 *
 *      Power-on test of integrity for the crypto module.
 */

#include <string.h>
#include <bootlib.h>
#include <efiutils.h>
#include <IndustryStandard/PeImage.h>

#include "crypto.h"

extern uint8_t _text, _etext, _rodata, _data, _edata, __executable_start;

#define HEADERS_SIZE 0x1000 // Must match uefi.lds

#if defined(only_em64t) || defined(only_arm64)
#define REL_BASED_PTR EFI_IMAGE_REL_BASED_DIR64
#else
#define REL_BASED_PTR EFI_IMAGE_REL_BASED_HIGHLOW
#endif

#define HASH_SIZE MBEDTLS_MD_MAX_SIZE

/*
 * Space reserved for an internal copy of this modules's .reloc section, used
 * to undo relocations while computing the integrity hash.  Initialized here to
 * a nonzero value to ensure it is not placed in the bss.  Filled in during the
 * build process by elf2efi.
 */
const uint16_t _reloc_copy[RELOC_COPY_SIZE / sizeof(uint16_t)] =
   { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0, /*...*/ };

/*
 * Expected value for the integrity hash.  Computed and filled in during the
 * build process by elf2efi.
 *
 * The linker script uefi.ld inserts this section at the end of the EFI .rodata
 * section, to make it easier to skip it in the hash computation.
 */
const uint8_t _expected_hash[HASH_SIZE]
   __attribute__ ((section (".integrity"))) = { 0xff, 0, /*...*/ };

/*
 * HMAC key used for the hash.  (Randomly generated.)
 *
 * 9cd1397275ea8e0b50d010aef1a84429fa7111c04aa39d877f3b8c02f2d84860
 * 30232c3c1f5ba0653207fd863c623ae74c8d9e641626391e8fc2e3805316a7e3
 */
const uint8_t _hmac_key[HASH_SIZE] = {
   0x9c, 0xd1, 0x39, 0x72, 0x75, 0xea, 0x8e, 0x0b, 0x50, 0xd0, 0x10, 0xae,
   0xf1, 0xa8, 0x44, 0x29, 0xfa, 0x71, 0x11, 0xc0, 0x4a, 0xa3, 0x9d, 0x87,
   0x7f, 0x3b, 0x8c, 0x02, 0xf2, 0xd8, 0x48, 0x60, 0x30, 0x23, 0x2c, 0x3c,
   0x1f, 0x5b, 0xa0, 0x65, 0x32, 0x07, 0xfd, 0x86, 0x3c, 0x62, 0x3a, 0xe7,
   0x4c, 0x8d, 0x9e, 0x64, 0x16, 0x26, 0x39, 0x1e, 0x8f, 0xc2, 0xe3, 0x80,
   0x53, 0x16, 0xa7, 0xe3
};

/*-- next_reloc ----------------------------------------------------------------
 *
 *      Get the address of the next relocation.
 *
 * Results
 *      Address
 *----------------------------------------------------------------------------*/
uint8_t *next_reloc(intptr_t slide)
{
   static uint32_t page_rva;
   static uint32_t n;
   static unsigned i;
   static uint8_t *reloc, *last_reloc;
   unsigned type, offset;
   EFI_IMAGE_BASE_RELOCATION *hdr;

   do {
      if (n == 0) {
         // Start parsing a new block
         if (i > ARRAYSIZE(_reloc_copy) - sizeof(*hdr)) {
            return NULL; // no more blocks
         }

         hdr = (EFI_IMAGE_BASE_RELOCATION *)&_reloc_copy[i];
         page_rva = hdr->VirtualAddress;
         n = hdr->SizeOfBlock / sizeof(uint16_t);
         if (n == 0) {
            return NULL; // no more blocks (reading slack space past end)
         }

         i += sizeof(*hdr) / sizeof(uint16_t);
         n -= sizeof(*hdr) / sizeof(uint16_t);
      }

      offset = _reloc_copy[i++];
      type = offset >> 12;
      offset &= 0xfff;
      n--;

   } while (type == EFI_IMAGE_REL_BASED_ABSOLUTE);

   if (type != REL_BASED_PTR) {
      failure("Unsupported relocation type");
   }

   reloc = (uint8_t *)(page_rva + offset + slide);
   if (reloc <= last_reloc) {
      failure("Relocations not in ascending order");
   }
   last_reloc = reloc;

   return reloc;
}

/*-- hash_section --------------------------------------------------------------
 *
 *      Add a section to the hash computation.
 *
 * Parameters
 *      IN/OUT md_ctx: context of ongoing hash computation
 *      IN/OUT nr:     next relocation
 *      IN start:      start address of section
 *      IN end:        end address of section
 *
 * Results
 *      EFI_SUCCESS, or an EFI error status.
 *----------------------------------------------------------------------------*/
void hash_section(mbedtls_md_context_t *md_ctx, uint8_t **nr,
                  const uint8_t *start, const uint8_t *end, intptr_t slide)
{
   int errcode;
   const uint8_t *p, *q;

   if (*nr == NULL) {
      *nr = next_reloc(slide);
   }
   p = start;

   while (p < end) {
      // Hash to next reloc, or to end if no reloc in range
      if (*nr >= start && *nr < end) {
         q = *nr;
      } else {
         q = end;
      }
      if (q != p) {
         errcode = mbedtls_md_hmac_update(md_ctx, p, q - p);
         if (errcode != 0) {
            failure("mbedtls_md_hmac_update error");
         }
         p = q;
      }

      // Hash next reloc if any
      if (q == *nr) {
         uintptr_t ptr;
         memcpy(&ptr, *nr, sizeof(ptr));
         ptr -= slide;
         errcode = mbedtls_md_hmac_update(md_ctx, (uint8_t *)&ptr, sizeof(ptr));
         if (errcode != 0) {
            failure("mbedtls_md_hmac_update error");
         }
         p = *nr + sizeof(ptr);
         *nr = next_reloc(slide);
      }
   }
}

/*-- hash_image ----------------------------------------------------------------
 *
 *      Hash the running image as it would appear with relocations undone.
 *
 * Parameters
 *      OUT hash:      computed hash
 *
 * Results
 *      Exits with an error upon failure.
 *----------------------------------------------------------------------------*/
void hash_image(uint8_t hash[HASH_SIZE])
{
   mbedtls_md_context_t md_ctx;
   const mbedtls_md_info_t *md_info;
   int errcode;
   uint8_t *nr = NULL;
   intptr_t slide = &__executable_start - (uint8_t *)HEADERS_SIZE;

   mbedtls_md_init(&md_ctx);
   md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA512);
   if (md_info == NULL) {
      failure("SHA512 implementation missing");
   }

   errcode = mbedtls_md_setup(&md_ctx, md_info, true);
   if (errcode != 0) {
      failure("mbedtls_md_setup error");
   }

   errcode = mbedtls_md_hmac_starts(&md_ctx, _hmac_key, sizeof(_hmac_key));
   if (errcode != 0) {
      failure("mbedtls_md_hmac_starts error");
   }

   // Hash .text
   hash_section(&md_ctx, &nr, &_text, &_etext, slide);

   // Hash .rodata except for _expected_hash
   hash_section(&md_ctx, &nr, &_rodata, _expected_hash, slide);

   // Hash .data
   hash_section(&md_ctx, &nr, &_data, &_edata, slide);

#if FORCE_INTEGRITY_FAIL
   // Miscompute the hash to provoke an integrity test failure
   mbedtls_md_hmac_update(&md_ctx, (uint8_t *)"junk", 4);
#endif

   errcode = mbedtls_md_hmac_finish(&md_ctx, hash);
   if (errcode != 0) {
      failure("mbedtls_md_hmac_finish error");
   }

   mbedtls_md_free(&md_ctx);
}

/*-- integrity_test ------------------------------------------------------------
 *
 *      Power-on test for image integrity.  Computes a hash of the image
 *      (undoing the expected relocations performed by UEFI) and checks that it
 *      matches the expected value.
 *
 * Results
 *      Exits with an error upon failure.
 *----------------------------------------------------------------------------*/
void integrity_test(void)
{
   uint8_t hash[HASH_SIZE];

   memset(hash, 0, HASH_SIZE);
   hash_image(hash);

   if (memcmp(hash, _expected_hash, HASH_SIZE) != 0) {
      /*
       * If the hash mismatches, exit with the computed hash in ExitData,
       * converted to a hex string.
       */
      unsigned i;
      char str[HASH_SIZE * 2 + 1];
      for (i = 0; i < HASH_SIZE; i++) {
         snprintf(&str[2 * i], 3, "%02x", hash[i]);
      }
      failure(str);
   }
}

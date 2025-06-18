/*******************************************************************************
 * Copyright (c) 2008-2022 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * elf.c -- ELF32/64 parsing functions.
 */

#include <elf.h>
#include "elf_int.h"
#include "mboot.h"

/*-- get_image_addr_range ------------------------------------------------------
 *
 *      Return the range of addresses described by an ELF image's program
 *      headers.
 *
 *      Range is [image_base, image_end).
 *
 * Parameters
 *      IN  buffer: pointer to the ELF binary buffer
 *      OUT image_base: beginning of range covered by ELF binary
 *      OUT image_end: end of range covered by ELF binary
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
void get_image_addr_range(void *buffer, Elf_CommonAddr *image_base,
                          Elf_CommonAddr *image_end)
{
   unsigned i;
   Elf_CommonEhdr *ehdr;
   Elf_CommonPhdr *phdr;
   Elf_CommonPhdr *ph;
   Elf_CommonAddr run_addr;
   Elf64_Size run_size;
   Elf_CommonAddr b = ULONG_MAX;
   Elf_CommonAddr e = 0;

   ehdr = buffer;
   phdr = (Elf_CommonPhdr *)((char *)buffer + Elf_CommonEhdrGetPhOff(ehdr));

   /*
    * Figure out current image start and end.
    */
   for (i = 0; i <  Elf_CommonEhdrGetPhNum(ehdr); i++) {
      ph = Elf_CommonEhdrGetPhdr(ehdr, phdr, i);

      if (Elf_CommonPhdrGetType(ehdr, ph) != PT_LOAD) {
         continue;
      }

      run_addr = Elf_CommonPhdrGetPaddr(ehdr, ph);
      run_size = Elf_CommonPhdrGetMemsz(ehdr, ph);

      if (b > run_addr) {
         b = run_addr;
      }

      if (e < (run_addr + run_size)) {
         e = run_addr + run_size;
      }
   }

   *image_base = b;
   *image_end = e;
}

/*-- is_valid_elf --------------------------------------------------------------
 *
 *      ELF32/64 sanity checks.
 *
 * Parameters
 *      IN buffer: pointer to the ELF binary buffer
 *      IN buflen: size of the buffer, in bytes
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int is_valid_elf(void *buffer, size_t buflen)
{
   Elf_CommonEhdr *ehdr = buffer;
   int status;
   bool is64;

   if (buflen < EI_NIDENT + 4 || !IS_ELF(*ehdr)) {
      return ERR_BAD_TYPE;
   }

   status = elf_arch_supported(buffer);
   if (status != ERR_SUCCESS) {
      return status;
   }

   is64 = Elf_CommonEhdrIs64(ehdr);

   if (buflen < Elf_CommonEhdrSize(is64)) {
      return ERR_UNEXPECTED_EOF;
   }

   if (Elf_CommonEhdrGetPhNum(ehdr) <= 0) {
      return ERR_BAD_HEADER;
   }

   if ((size_t)Elf_CommonEhdrGetPhNum(ehdr) * Elf_CommonPhdrSize(is64)
       + Elf_CommonEhdrGetPhOff(ehdr) > buflen) {
      return ERR_UNEXPECTED_EOF;
   }

   if (ehdr->e_ident[EI_VERSION] != EV_CURRENT ||
       Elf_CommonEhdrGetVersion(ehdr) != EV_CURRENT) {
      return WARNING(ERR_INCOMPATIBLE_VERSION);
   }

   if (Elf_CommonEhdrGetPhEntSize(ehdr) != Elf_CommonPhdrSize(is64)) {
      return WARNING(ERR_BAD_HEADER);
   }

   return ERR_SUCCESS;
}

/*-- elf_check_headers ---------------------------------------------------------
*
*      Parse ELF32/64 headers and make sure no segment overruns end of file.
*
* Parameters
*      IN buffer: pointer to the ELF binary buffer
*      IN buflen: size of the buffer, in bytes
*      OUT base:  beginning of text/data (from PHDR with lowest load addr)
*
* Results
*      ERR_SUCCESS, or a generic error status.
*----------------------------------------------------------------------------*/
int elf_check_headers(void *buffer, size_t buflen, Elf_CommonAddr *base)
{
   Elf_CommonEhdr *ehdr = buffer;
   Elf_CommonPhdr *phdr, *ph;
   Elf64_Size load_size;
   Elf_CommonAddr b;
   char *load_addr;
   unsigned int i;
   int status;

   status = is_valid_elf(buffer, buflen);
   if (status != ERR_SUCCESS && !IS_WARNING(status)) {
      return status;
   }

   b = (Elf_CommonAddr) -1;
   phdr = (Elf_CommonPhdr *)((char *)buffer + Elf_CommonEhdrGetPhOff(ehdr));

   for (i = 0; i < Elf_CommonEhdrGetPhNum(ehdr); i++) {
      ph = Elf_CommonEhdrGetPhdr(ehdr, phdr, i);

      if (Elf_CommonPhdrGetType(ehdr, ph) == PT_LOAD) {
         load_addr = (char *)buffer + Elf_CommonPhdrGetOffset(ehdr, ph);
         load_size = Elf_CommonPhdrGetFilesz(ehdr, ph);

         if ((Elf_CommonAddr) (uintptr_t) load_addr < b) {
           b = (Elf_CommonAddr) (uintptr_t) load_addr;
         }

         if (load_addr + load_size > (char *)buffer + buflen) {
            return ERR_UNEXPECTED_EOF;
         }
      }
   }

   if (status == ERR_SUCCESS && base != NULL) {
      *base = b;
   }

   return status;
}

/*-- elf_register_segments -----------------------------------------------------
 *
 *      Register ELF image segments for relocation.
 *
 * Parameters
 *      IN  ehdr:       ELF header.
 *      IN  run_addend: used to compute actual segment loaded address.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int elf_register_segments(Elf_CommonEhdr *ehdr,
                                 Elf_CommonAddr run_addend)
{
   int status;
   unsigned int i;
   char *load_addr;
   Elf64_Size run_size;
   Elf64_Size bss_size;
   Elf64_Size load_size;
   Elf_CommonPhdr *ph;
   Elf_CommonPhdr *phdr;
   Elf_CommonAddr run_addr;

   phdr = (Elf_CommonPhdr *)((char *) ehdr + Elf_CommonEhdrGetPhOff(ehdr));
   for (i = 0; i < Elf_CommonEhdrGetPhNum(ehdr); i++) {
      ph = Elf_CommonEhdrGetPhdr(ehdr, phdr, i);

      if (Elf_CommonPhdrGetType(ehdr, ph) != PT_LOAD) {
         continue;
      }

      load_addr = (char *) ehdr + Elf_CommonPhdrGetOffset(ehdr, ph);
      load_size = Elf_CommonPhdrGetFilesz(ehdr, ph);
      run_addr = Elf_CommonPhdrGetPaddr(ehdr, ph) + run_addend;
      run_size = Elf_CommonPhdrGetMemsz(ehdr, ph);
      bss_size = run_size - load_size;

      if (boot.debug) {
         Log(LOG_DEBUG, "[k] %"PRIx64" - %"PRIx64" -> %"PRIx64
             " - %"PRIx64" (%"PRIu64" bytes)",
             PTR_TO_UINT64(load_addr), PTR_TO_UINT64(load_addr) + load_size,
             run_addr, run_addr + run_size, run_size);
      }

      /* Set up the segment */
      if (run_size - bss_size > 0) {
         status = add_kernel_object(load_addr, run_size - bss_size, run_addr);
         if (status != ERR_SUCCESS) {
            return status;
         }
      }

      /* Set up the BSS */
      if (bss_size > 0) {
         status = add_kernel_object(NULL, bss_size, run_addr + load_size);
         if (status != ERR_SUCCESS) {
            return status;
         }
      }
   }

   return ERR_SUCCESS;
}


/*-- elf_register --------------------------------------------------------------
 *
 *      Register an ELF image for relocation.
 *
 * Parameters
 *      IN  buffer: pointer to the ELF binary buffer
 *      OUT entry:  ELF entry point
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int elf_register(void *buffer, Elf_CommonAddr *entry)
{
   Elf_CommonEhdr *ehdr = buffer;
   Elf_CommonAddr run_addend;
   Elf_CommonAddr link_base;
   Elf_CommonAddr link_end;
   int status;

   get_image_addr_range(buffer, &link_base, &link_end);
   Log(LOG_DEBUG, "ELF link address range is [0x%"PRIx64":0x%"PRIx64")\n",
       link_base, link_end);

   status = elf_arch_alloc(link_base, link_end - link_base, &run_addend);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = elf_register_segments(ehdr, run_addend);
   if (status != ERR_SUCCESS) {
      return status;
   }

   *entry = Elf_CommonEhdrGetEntry(ehdr) + run_addend;
   return ERR_SUCCESS;
}


#if defined(only_arm64) || defined(only_riscv64)
/*-- elf_arch_alloc_anywhere ---------------------------------------------------
 *
 *      Allocate away the memory ranges that will contain the ELF image
 *      post relocation.
 *
 *      AARCH64/RISCV64 binaries can be loaded anywhere, provided the alignment
 *      requirements have been met. This means that the ranges allocated may be
 *      different from the image linked address, with a non-zero reported
 *      addend.
 *
 * Parameters
 *      IN  link_base:  image base address.
 *      IN  link_size:  image size.
 *      IN  align:      allocation alignment.
 *      OUT run_addend: used to calculate where the ELF binary will
 *                      be relocated to.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int elf_arch_alloc_anywhere(Elf_CommonAddr link_base, Elf64_Size link_size,
                            size_t align, Elf_CommonAddr *run_addend)
{
   int status;
   Elf_CommonAddr reloc_base;

   status = runtime_alloc(&reloc_base, link_size, align, ALLOC_ANY);
   if (status != ERR_SUCCESS) {
      return status;
   }

   Log(LOG_DEBUG, "Reloc range is [0x%"PRIx64":0x%"PRIx64")\n",
       reloc_base, reloc_base + link_size);
   *run_addend = reloc_base - link_base;
   return ERR_SUCCESS;
}
#endif /* defined(only_arm64) || defined(only_riscv64) */

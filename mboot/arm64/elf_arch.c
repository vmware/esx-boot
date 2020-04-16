/*******************************************************************************
 * Copyright (c) 2017-2018 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * elf_arch.c -- Architecture-specific ELF handling.
 */

#include <elf.h>
#include "elf_int.h"
#include "mboot.h"

#define ELF_EXEC_ALIGNMENT 0x200000

/*-- elf_arch_supported --------------------------------------------------------
 *
 *      Validates ELF header against architecture requirements.
 *
 * Parameters
 *      IN buffer: pointer to the ELF binary buffer
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int elf_arch_supported(void *buffer)
{
   Elf_CommonEhdr *ehdr = buffer;

   if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
      return ERR_BAD_ARCH;
   }

   if (Elf_CommonEhdrGetMachine(ehdr) != EM_AARCH64 ||
       ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
      return ERR_BAD_ARCH;
   }

   if (Elf_CommonEhdrGetType(ehdr) != ET_EXEC &&
       Elf_CommonEhdrGetType(ehdr) != ET_DYN) {
      return WARNING(ERR_NOT_EXECUTABLE);
   }

   return ERR_SUCCESS;
}

/*-- elf_arch_alloc ------------------------------------------------------------
 *
 *      Allocate away the memory ranges that will contain the ELF image
 *      post relocation.
 *
 *      AArch64 binaries can be loaded anywhere, provided the alignment
 *      requirements have been met. This means that the ranges allocated
 *      may be different from the image linked address, with a non-zero
 *      reported addend.
 *
 * Parameters
 *      IN  link_base:  image base address.
 *      IN  link_size:  image size.
 *      OUT run_addend: used to calculate where the ELF binary will
 *                      be relocated to.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int elf_arch_alloc(Elf_CommonAddr link_base, Elf64_Size link_size,
                   Elf_CommonAddr *run_addend)
{
   int status;
   Elf_CommonAddr reloc_base;

   status = runtime_alloc(&reloc_base, link_size,
                          ELF_EXEC_ALIGNMENT, ALLOC_ANY);
   if (status != ERR_SUCCESS) {
      return status;
   }

   Log(LOG_DEBUG, "Reloc range is [0x%lx:0x%lx)\n", reloc_base, reloc_base +
       link_size);
   *run_addend = reloc_base - link_base;
   return ERR_SUCCESS;
}

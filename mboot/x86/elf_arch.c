/*******************************************************************************
 * Copyright (c) 2017 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * elf_arch.c -- Architecture-specific ELF handling.
 */

#include <elf.h>
#include "elf_int.h"
#include "mboot.h"

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

   if (ehdr->e_ident[EI_CLASS] != ELFCLASS32 &&
       ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
      return ERR_BAD_ARCH;
   }

   if ((Elf_CommonEhdrGetMachine(ehdr) != EM_386 &&
        Elf_CommonEhdrGetMachine(ehdr) != EM_X86_64) ||
       ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
      return ERR_BAD_ARCH;
   }

   if (Elf_CommonEhdrGetType(ehdr) != ET_EXEC) {
      return WARNING(ERR_NOT_EXECUTABLE);
   }

   return ERR_SUCCESS;
}

/*-- elf_arch_alloc ------------------------------------------------------------
 *
 *      Allocate away the memory ranges that will contain the ELF image
 *      post relocation.
 *
 *      x86 binaries must be loaded at the linked address, thus reported
 *      addend is always zero.
 *
 * Parameters
 *      IN  link_base:  image base address.
 *      IN  link_size:  image size.
 *      OUT run_addend: 0.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int elf_arch_alloc(Elf_CommonAddr link_base, Elf64_Size link_size,
                   Elf_CommonAddr *run_addend)
{
   int status;

   status = runtime_alloc_fixed(&link_base, link_size);
   if (status != ERR_SUCCESS) {
      return status;
   }

   *run_addend = 0;
   return ERR_SUCCESS;
}

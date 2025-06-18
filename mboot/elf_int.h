/*******************************************************************************
 * Copyright (c) 2017-2022 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * elf_int.h -- elf.c and elf_arch.c internal header file.
 */

#ifndef ELF_INT_H_
#define ELF_INT_H_

int elf_arch_supported(void *buffer);
int elf_arch_alloc(Elf_CommonAddr link_base, Elf64_Size link_size,
                   Elf_CommonAddr *run_addend);
#if defined(only_arm64) || defined(only_riscv64)
int elf_arch_alloc_anywhere(Elf_CommonAddr link_base, Elf64_Size link_size,
                            size_t align, Elf_CommonAddr *run_addend);
#endif /* defined(only_arm64) || defined(only_riscv64) */

#endif /* !ELF_INT_H_ */

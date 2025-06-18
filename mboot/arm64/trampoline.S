#******************************************************************************
# Copyright (c) 2016-2021 Broadcom. All Rights Reserved.
# The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0
#******************************************************************************

# trampoline.S
#
#      All the code in this module will be relocated in order to execute from
#      safe memory. That is to ensure that it will never get overwritten when
#      processing the relocations.
#
#      - THIS CODE MUST LIVE IN CONTIGUOUS MEMORY
#      - THIS CODE MUST BE POSITION-INDEPENDENT
#      - THIS CODE ASSUMES THAT HARDWARE INTERRUPTS ARE DISABLED

#
# The following OFFSET_ fields need to match trampoline.inc
# and handoff_t state.
#
#define OFFSET_STACK          (0 * 8)
#define OFFSET_RELOCS         (1 * 8)
#define OFFSET_RELOCATE       (2 * 8)
#define OFFSET_MBI            (3 * 8)
#define OFFSET_KERNEL         (4 * 8)
#define OFFSET_TRLOW          (5 * 8)
#define OFFSET_MAGIC          (6 * 8)
#define TRAMPOLINE_STACK_SIZE 0x2000

#-- trampoline ----------------------------------------------------------------
#
#      This is the function that actually processes the relocations, sets up an
#      ESXBootInfo compliant environment and jumps to the kernel.
#
# Prototype
#      void relocate(handoff_t *handoff);
#
# Side effects:
#      None.
#------------------------------------------------------------------------------
.globl trampoline
.section .trampoline
trampoline:
      mov      x20, x0
      ldr      x0, [x20, #OFFSET_STACK]
      add      x0, x0, #TRAMPOLINE_STACK_SIZE
      bic      sp, x0, #0xf

#
# do_reloc(reloc_t *)
#
      ldr      x0, [x20, #OFFSET_RELOCS]
      ldr      x1, [x20, #OFFSET_RELOCATE]
      blr      x1

#
# handoff->kernel(x0, x1)
#
      ldr      x0, [x20, #OFFSET_MAGIC]
      ldr      x1, [x20, #OFFSET_MBI]
      ldr      x2, [x20, #OFFSET_KERNEL]

      blr      x2
1:    b        1b

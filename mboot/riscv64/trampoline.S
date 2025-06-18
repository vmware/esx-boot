#******************************************************************************
# Copyright (c) 2022 Broadcom. All Rights Reserved.
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
      mv       s1, a0
      ld       a0, OFFSET_STACK(s1)
      li       t0, TRAMPOLINE_STACK_SIZE
      add      a0, a0, t0
      andi     sp, a0, -16

#
# do_reloc(reloc_t *)
#
      ld       a0, OFFSET_RELOCS(s1)
      ld       a1, OFFSET_RELOCATE(s1)
      jalr     a1

#
# handoff->kernel(a0, x1)
#
      ld       a0, OFFSET_MAGIC(s1)
      ld       a1, OFFSET_MBI(s1)
      ld       a2, OFFSET_KERNEL(s1)

      jr       a2

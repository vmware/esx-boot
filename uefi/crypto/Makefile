#*******************************************************************************
# Copyright (c) 2015-2024 Broadcom. All Rights Reserved.
# The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# Crypto Makefile
#

TOPDIR      := ../..
include common.mk

SRC         := crypto.c integrity.c selftest.c

BASENAME    := crypto
TARGETTYPE  := bsdrv
INC         := $(UEFIINC)
INC         += $(CRYPTOINC)
LIBS        := $(FIRMLIB) $(LIBC) $(CRYPTOLIB) $(LIBGCC) $(BOOTLIB)

ifeq ($(DEBUG),1)
LIBS        += $(LIBUART) $(FDTLIB)
endif
CDEF        += $(CRYPTOTEST)

# Hint: Adjust RELOC_COPY_SIZE upward if elf2efi fails
RELOC_COPY_SIZE := 3000
CFLAGS          += -DRELOC_COPY_SIZE=$(RELOC_COPY_SIZE)
ELF2EFIFLAGS    := --copy-reloc $(RELOC_COPY_SIZE) --insert-hash

include rules.mk

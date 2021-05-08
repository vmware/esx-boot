#*******************************************************************************
# Copyright (c) 2015,2020 VMware, Inc.  All rights reserved.
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
LIBS        := $(FIRMLIB) $(LIBC) $(CRYPTOLIB) $(LIBGCC)

# Hint: Adjust RELOC_COPY_SIZE upward if elf2efi fails
RELOC_COPY_SIZE := 2500
CFLAGS          += -DRELOC_COPY_SIZE=$(RELOC_COPY_SIZE)
ELF2EFIFLAGS    := --copy-reloc $(RELOC_COPY_SIZE) --insert-hash

include rules.mk

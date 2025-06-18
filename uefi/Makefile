#*******************************************************************************
# Copyright (c) 2008-2020 Broadcom. All Rights Reserved.
# The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

TOPDIR    := ..
include common.mk

MAKEFLAGS += -I ../../env

SUBDIRS := efiutils    \
           elf2efi     \
           iso9660     \
           isobounce   \
           menu        \
           frobosboot

ifeq ($(CRYPTO_MODULE), 1)
   SUBDIRS += crypto
endif

# Building rules
.PHONY: all $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	@$(MAKE) -C $@

# Dependencies
iso9660: elf2efi efiutils
isobounce: elf2efi efiutils
menu: elf2efi efiutils
frobosboot: elf2efi efiutils
crypto: elf2efi efiutils

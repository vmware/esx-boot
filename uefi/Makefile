#*******************************************************************************
# Copyright (c) 2008-2011,2015-2016,2020 VMware, Inc.  All rights reserved.
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

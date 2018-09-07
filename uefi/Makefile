#*******************************************************************************
# Copyright (c) 2008-2011,2015-2016 VMware, Inc.  All rights reserved.
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

# Building rules
.PHONY: all $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	@$(MAKE) -C $@

# Dependencies
iso9660: elf2efi efiutils
isobounce: elf2efi efiutils

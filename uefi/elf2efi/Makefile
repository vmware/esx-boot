#*******************************************************************************
# Copyright (c) 2008-2011,2014-2015 VMware, Inc.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

TOPDIR    := ../..
include common.mk

INC       := $(HOST_LIBBFDINC) $(UEFIINC)
BUILD_DIR := $(TOOLS_DIR)

.PHONY: all $(BUILD_DIR)

all: $(BUILD_DIR) $(ELF2EFI)

ifeq ($(ARCH),ia32)
   CFLAGS := -m32
else
   #
   # Our private BFD is 64 bit, so the deliverable
   # needs to be 64-bit as well.
   #
   CFLAGS := -m64
endif

$(ELF2EFI): elf2efi.c $(HOST_LIBBFD) $(HOST_LIBERTY)
	$(call print,CC,$@)
	$(HOST_CC) $(CFLAGS) -O2 $(patsubst %,-I%,$(INC)) -o $@ $^

$(BUILD_DIR):
	$(call MKDIR,$@)

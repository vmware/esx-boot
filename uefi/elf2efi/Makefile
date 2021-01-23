#*******************************************************************************
# Copyright (c) 2008-2011,2014-2015,2019-2020 VMware, Inc.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

TOPDIR    := ../..
include common.mk

INC       := $(HOST_LIBBFDINC) $(HOST_LIBSSLINC) $(UEFIINC) 
BUILD_DIR := $(TOOLS_DIR)
HOST_CFLAGS += -Wno-stringop-truncation

.PHONY: all $(BUILD_DIR)

all: $(BUILD_DIR) $(ELF2EFI)

$(ELF2EFI): elf2efi.c $(HOST_LIBBFD) $(HOST_LIBERTY) $(HOST_LIBCRYPTO)
	$(call print,CC,$@)
	$(HOST_CC) $(HOST_CFLAGS) -O2 $(patsubst %,-I%,$(INC)) -o $@ $^ \
		-lpthread -ldl

$(BUILD_DIR):
	$(call MKDIR,$@)

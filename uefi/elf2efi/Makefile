#*******************************************************************************
# Copyright (c) 2008-2022 Broadcom. All Rights Reserved.
# The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

TOPDIR    := ../..
include common.mk

INC       := $(HOST_LIBSSLINC) $(UEFIINC)
BUILD_DIR := $(TOOLS_DIR)
HOST_CFLAGS += -Wno-stringop-truncation

.PHONY: all $(BUILD_DIR)

all: $(BUILD_DIR) $(ELF2EFI)

$(ELF2EFI): elf2efi.c
	$(call print,CC,$@)
	$(HOST_CC) $(HOST_CFLAGS) -O2 $(patsubst %,-I%,$(INC)) -o $@ $^ \
		$(HOST_LIBCRYPTO) -lpthread -ldl

$(BUILD_DIR):
	$(call MKDIR,$@)

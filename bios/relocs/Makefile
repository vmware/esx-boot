#*******************************************************************************
# Copyright (c) 2008-2019 Broadcom. All Rights Reserved.
# The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

TOPDIR   := ../..
include common.mk

BUILD_DIR := $(TOOLS_DIR)

.PHONY: all $(BUILD_DIR)

all: $(BUILD_DIR) $(RELOCS)

$(RELOCS): relocs.c
	$(call print,HOST_CC,$@)
	$(HOST_CC) $(HOST_CFLAGS) -O2 -o $@ $^

$(BUILD_DIR):
	$(call MKDIR,$@)

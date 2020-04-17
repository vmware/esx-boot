#*******************************************************************************
# Copyright (c) 2008-2011,2015,2019 VMware, Inc.  All rights reserved.
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

#*******************************************************************************
# Copyright (c) 2015-2022 Broadcom. All Rights Reserved.
# The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

TOPDIR    := ..
include common.mk

MAKEFLAGS += -I ../../env

SUBDIRS := test_acpi test_libuart test_gui test_smbios test_libc \
	   test_runtimewd test_malloc

ifneq ($(BUILDENV),com32)
SUBDIRS += test_rts
endif

ifneq ($(IARCH),x86)
SUBDIRS += test_fdt
endif

# Building rules
.PHONY: all $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	@$(MAKE) -C $@

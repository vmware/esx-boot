#*******************************************************************************
# Copyright (c) 2015-2016,2021-2022 VMware, Inc.  All rights reserved.
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

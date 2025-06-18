#*******************************************************************************
# Copyright (c) 2008-2015 Broadcom. All Rights Reserved.
# The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

TOPDIR    := ..
include common.mk

MAKEFLAGS += -I ../../env

SUBDIRS := com32       \
           ifgpxe      \
           relocs

# Building rules
.PHONY: all $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	@$(MAKE) -C $@

ifgpxe: com32 relocs

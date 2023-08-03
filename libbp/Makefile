#*******************************************************************************
# Copyright (c) 2022 VMware, Inc.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# Libbp Makefile
#

TOPDIR       := ..
include common.mk

ifneq ($(ARCH),ia32)
SRC         := bpatch.c \
	            bapply.c \
               $(IARCH)/bpatch_arch.c

INC += $(IARCH)
endif

BASENAME    := bp
TARGETTYPE  := lib

include rules.mk

#*******************************************************************************
# Copyright (c) 2020 VMware, Inc.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# test_fdt Makefile
#

TOPDIR      := ../..
include common.mk

SRC         := test_fdt.c

BASENAME    := test_fdt
TARGETTYPE  := app
LIBS        := $(BOOTLIB) $(ENV_LIB) $(FDTLIB)
INC         := $(FDTINC)

CFLAGS      +=

include rules.mk

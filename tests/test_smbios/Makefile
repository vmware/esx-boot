#*******************************************************************************
# Copyright (c) 2017 VMware, Inc.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# test_smbios Makefile
#

TOPDIR      := ../..
include common.mk

SRC         := test_smbios.c

BASENAME    := test_smbios
TARGETTYPE  := app
LIBS        := $(BOOTLIB) $(ENV_LIB)
CFLAGS      += -pedantic

include rules.mk

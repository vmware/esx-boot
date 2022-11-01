#*******************************************************************************
# Copyright (c) 2021 VMware, Inc.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# test_acpi Makefile
#

TOPDIR      := ../..
include common.mk

SRC         := test_acpi.c

BASENAME    := test_acpi
TARGETTYPE  := app
LIBS        := $(BOOTLIB) $(ENV_LIB)
CFLAGS      += -pedantic

include rules.mk

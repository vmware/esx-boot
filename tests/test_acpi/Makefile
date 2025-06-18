#*******************************************************************************
# Copyright (c) 2021 Broadcom. All Rights Reserved.
# The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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

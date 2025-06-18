#*******************************************************************************
# Copyright (c) 2015-2024 Broadcom. All Rights Reserved.
# The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# Menu Makefile
#

TOPDIR      := ../..
include common.mk

SRC         := menu.c

BASENAME    := menu
TARGETTYPE  := app
INC         := $(UEFIINC)
INC         += $(CRYPTOINC)
LIBS        := $(ENV_LIB) $(BOOTLIB)

ELF2EFIFLAGS    := --insert-hash

include rules.mk

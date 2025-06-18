#*******************************************************************************
# Copyright (c) 2015-2017 Broadcom. All Rights Reserved.
# The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# frobosboot Makefile
#

TOPDIR      := ../..
include common.mk

SRC         := frobosboot.c   \
               config.c       \
               ui.c

BASENAME    := frobosboot
TARGETTYPE  := app
INC         := $(UEFIINC)
LIBS        := $(ENV_LIB) $(BOOTLIB)

include rules.mk

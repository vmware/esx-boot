#*******************************************************************************
# Copyright (c) 2008-2011 Broadcom. All Rights Reserved.
# The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# Safeboot Makefile
#

TOPDIR      := ..
include common.mk

SRC         := bootbank.c     \
               chainload.c    \
               config.c       \
               gui.c          \
               safeboot.c     \
               vmfat.c

BASENAME    := safeboot
TARGETTYPE  := app
LIBS        := $(BOOTLIB) $(ENV_LIB)
CFLAGS      += -pedantic

include rules.mk

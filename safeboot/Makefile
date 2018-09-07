#*******************************************************************************
# Copyright (c) 2008-2011 VMware, Inc.  All rights reserved.
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

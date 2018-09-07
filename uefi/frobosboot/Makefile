#*******************************************************************************
# Copyright (c) 2015-2017 VMware, Inc.  All rights reserved.
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

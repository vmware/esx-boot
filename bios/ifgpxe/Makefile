#*******************************************************************************
# Copyright (c) 2008-2011 VMware, Inc.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# ifgpxe Makefile
#

TOPDIR      := ../..
include common.mk

SRC         := ifgpxe.c

BASENAME    := ifgpxe
TARGETTYPE  := app
INC         := $(TOPDIR)/bios/com32
LIBS        := $(ENV_LIB) $(BOOTLIB)

include rules.mk

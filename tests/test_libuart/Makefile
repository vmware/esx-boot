#*******************************************************************************
# Copyright (c) 2015 VMware, Inc.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# test_libuart Makefile
#

TOPDIR      := ../..
include common.mk

SRC         := test_libuart.c

BASENAME    := test_libuart
TARGETTYPE  := app
LIBS        := $(BOOTLIB) $(ENV_LIB)
CFLAGS      += -pedantic

include rules.mk

#*******************************************************************************
# Copyright (c) 2016 VMware, Inc.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# test_rts Makefile
#

TOPDIR      := ../..
include common.mk

SRC         := test_rts.c

BASENAME    := test_rts
TARGETTYPE  := app
LIBS        := $(BOOTLIB) $(ENV_LIB)
CFLAGS      += -pedantic

include rules.mk

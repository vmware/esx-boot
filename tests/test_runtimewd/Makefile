#*******************************************************************************
# Copyright (c) 2021 VMware, Inc.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# test_runtime_wd Makefile
#

TOPDIR      := ../..
include common.mk

SRC         := test_runtimewd.c

BASENAME    := test_runtimewd
TARGETTYPE  := app
LIBS        := $(ENV_LIB) $(BOOTLIB)

INC         := $(UEFIINC) ./

CGLAGS      += -pendantic

include rules.mk

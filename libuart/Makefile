#*******************************************************************************
# Copyright (c) 2008-2022 Broadcom. All Rights Reserved.
# The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# Libuart Makefile
#

TOPDIR       := ..
include common.mk

SRC         := uart.c \
               ns16550.c \
               $(IARCH)/uart_arch.c

ifeq ($(ARCH),arm64)
SRC += $(ARCH)/pl011.c
SRC += $(ARCH)/tmfifo.c
SRC += $(ARCH)/aapl-s5l.c
endif

BASENAME    := uart
TARGETTYPE  := lib

include rules.mk

#*******************************************************************************
# Copyright (c) 2008-2011,2020 VMware, Inc.  All rights reserved.
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
endif

BASENAME    := uart
TARGETTYPE  := lib

include rules.mk

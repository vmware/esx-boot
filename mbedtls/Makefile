#*******************************************************************************
# Copyright (c) 2015,2017 VMware, Inc.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# mbedtls Makefile
#

TOPDIR       := ..
include common.mk

SRC         := asn1parse.c \
               bignum.c \
               md.c \
               md_wrap.c \
               oid.c \
               rsa.c \
               sha256.c \
               sha512.c

BASENAME    := mbedtls
TARGETTYPE  := lib
CFLAGS      += -Dasm=__asm__

include rules.mk

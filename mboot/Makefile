#*******************************************************************************
# Copyright (c) 2008-2024 Broadcom. All Rights Reserved.
# The term "Broadcom" refers to Broadcom Inc.
# and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0
#*******************************************************************************

#
# Mboot Makefile
#

TOPDIR      := ..
include common.mk

SRC         := acpi.c	             \
               config.c              \
               elf.c                 \
               $(IARCH)/elf_arch.c   \
               gui.c                 \
               load.c                \
               mboot.c               \
               $(IARCH)/multiboot.c  \
               esxbootinfo.c         \
               $(IARCH)/esxbootinfo_arch.c \
               reloc.c               \
               secure.c              \
               system.c              \
               $(IARCH)/system_arch.c\
               trampoline.c          \
               signinfo.json         \
					cxl.c                 \

ifeq ($(IARCH),x86)
SRC += $(IARCH)/trampoline.s
else
SRC += fdt.c
SRC += $(IARCH)/trampoline.S
endif

TARGETTYPE  := app
INC += .

LIBS := $(BOOTLIB) $(ENV_LIB)

ifeq (,$(EKEY))
   BASENAME := mboot
else
   BASENAME := mboot-$(EKEY)
   CFLAGS   += -D$(EKEY) -DSECURE_BOOT
   INC      += $(UEFIINC) $(CRYPTOINC)
   LIBS     += $(CRYPTOLIB)

   ifeq ($(CRYPTO_MODULE), 1)
      CFLAGS += -DCRYPTO_MODULE
   endif
endif

include rules.mk

$(ODIR)/trampoline.o: trampoline.c
	$(call printcmd,CC)
	$(CC) $(CFLAGS) -fno-stack-protector -c -o $@ $<

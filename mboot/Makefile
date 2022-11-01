#*******************************************************************************
# Copyright (c) 2008-2017,2020-2021 VMware, Inc.  All rights reserved.
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

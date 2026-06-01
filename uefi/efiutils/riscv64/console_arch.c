/*******************************************************************************
 * Copyright (c) 2008-2025 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * console_arch.c -- EFI console management, architecture specific portions.
 */

#include <stdio.h>
#include <stdarg.h>
#include <io.h>
#include "efi_private.h"


/*-- get_serial_port -----------------------------------------------------------
 *
 *      Get the I/O base address of a serial port.
 *
 * Parameters
 *      IN  com:  unused
 *      OUT type: serial port type
 *      OUT io:   serial port base address
 *      OUT original_baudrate: current baudrate from firmware
 *
 * Results
 *      A generic error status.
 *----------------------------------------------------------------------------*/
int get_serial_port(UNUSED_PARAM(int com),
                    UNUSED_PARAM(serial_type_t *type),
                    UNUSED_PARAM(io_channel_t *io),
                    UNUSED_PARAM(uint32_t *original_baudrate))
{
   return ERR_NOT_FOUND;
}

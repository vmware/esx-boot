/*******************************************************************************
 * Copyright (c) 2013 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * timer.c -- Timer related functions
 */

#include <bootlib.h>
#include "com32_private.h"

/*-- int1a_read_current_ticks -------------------------------------------------
 *
 *      Get the number of ticks that have taken place since power on or reset.
 *      The high-order portion of the tick count is placed in CX and the
 *      low-order portion is placed in DX.
 *      AL = 00H if midnight has not passed since the last time the clock value
 *      was read or set. AL = 01H if midnight has passed, reading the tick value
 *      always resets the midnight signal.
 *
 * Parameters
 *      OUT current_ticks:          Number of ticks that have elapsed since the
 *                                  last read or reset/power-on.
 *      IN consider_timer_overflow: Flag indicating whether to consider the
 *                                  timer overflow.
 *----------------------------------------------------------------------------*/
static void int1a_read_current_ticks(uint32_t *current_ticks,
                                     bool consider_timer_overflow)
{
   com32sys_t oregs;

   intcall(0x1A, NULL, &oregs);

   *current_ticks = ((uint32_t)oregs.ecx.w[0] << 16) | (uint32_t)oregs.edx.w[0];

   if (consider_timer_overflow) {
      if (oregs.eax.b[0] != 0) {
         /* Midnight has passed */
         *current_ticks += PIT8254_MAX_TICK_VALUE;
      }
   }
}

/*-- firmware_get_time_ms -----------------------------------------------------
 *
 *      Get the current time in milliseconds that have elapsed since midnight.
 *
 * Parameters
 *      IN consider_timer_overflow: Flag indicating whether to consider the
 *                                  timer overflow.
 *
 * Results
 *      Time in milliseconds that have elapsed since midnight.
 *----------------------------------------------------------------------------*/
uint64_t firmware_get_time_ms(bool consider_timer_overflow)
{
   uint32_t ticks;

   ticks = 0;
   int1a_read_current_ticks(&ticks, consider_timer_overflow);

   return BIOS_TICKS_TO_MILLISEC(ticks);
}

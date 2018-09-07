/*******************************************************************************
 * Copyright (c) 2013 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * timer.c -- EFI timer access support
 */

#include "efi_private.h"

/*-- firmware_get_time_ms -----------------------------------------------------
 *
 *      Get the current time in milliseconds that have elapsed since
 *      00:00:00 Jan 1st 1900.
 *      According to the UEFI Specification 2.3, GetTime() Runtime Service in
 *      section 7.3, the valid range of years returned by GetTime() is
 *      1900-9999.
 *
 * Parameters
 *      IN consider_timer_overflow: Flag indicating whether to consider the
 *                                  timer overflow.
 * Results
 *      Time in milliseconds that have elapsed since 00:00:00 Jan 1st 1900.
 *----------------------------------------------------------------------------*/
uint64_t firmware_get_time_ms(UNUSED_PARAM(bool consider_timer_overflow))
{
   EFI_TIME Time;
   EFI_STATUS Status;
   uint64_t time;
   unsigned int year;

   /* The below array is used to find out how many days have elapsed till the
    * given month from January. The array takes into account for leap years
    */
   static const uint32_t total_days[2][13] = {
   {0,
    31,
    31 + 28,
    31 + 28 + 31,
    31 + 28 + 31 + 30,
    31 + 28 + 31 + 30 + 31,
    31 + 28 + 31 + 30 + 31 + 30,
    31 + 28 + 31 + 30 + 31 + 30 + 31,
    31 + 28 + 31 + 30 + 31 + 30 + 31 + 31,
    31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30,
    31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31,
    31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30,
    31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30 + 31},
   {0,
    31,
    31 + 29,
    31 + 29 + 31,
    31 + 29 + 31 + 30,
    31 + 29 + 31 + 30 + 31,
    31 + 29 + 31 + 30 + 31 + 30,
    31 + 29 + 31 + 30 + 31 + 30 + 31,
    31 + 29 + 31 + 30 + 31 + 30 + 31 + 31,
    31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30,
    31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31,
    31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30,
    31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30 + 31}};

   EFI_ASSERT(rs != NULL);
   EFI_ASSERT(rs->GetTime != NULL);
   Status = rs->GetTime(&Time, NULL);
   if (EFI_ERROR(Status)) {
      /* GetTime() may fail in certain cases, such as if the RTC's battery backup
       * has failed.
       */
      Log(LOG_WARNING, "Failed to get system time, the timer device"
          " may have a hardware problem\n");
      return 0;
   }

   /* Sanity Checks */
   if (Time.Year < 1900 || Time.Year > 9999 ||  // 1900 - 9999
       Time.Month == 0  || Time.Month > 12 ||  // 1 - 12
       Time.Day == 0 || Time.Day > 31 ||      // 1 - 31
       Time.Hour > 23 ||                     // 0 - 23
       Time.Minute > 59 ||                  // 0 - 59
       Time.Second > 59 ||                 // 0 - 59
       Time.Nanosecond > 999999999) {     // 0 - 999999999
      Log(LOG_WARNING, "Invalid system time obtained from timer device\n");
      return 0;
   }

   time = 0;
   for (year = 1900; year < Time.Year; year++) {
      time += total_days[IS_LEAP_YEAR(year)][12] * SECS_PER_DAY;
   }
   time += total_days[IS_LEAP_YEAR(year)][Time.Month - 1] * SECS_PER_DAY;
   time += (Time.Day - 1) * SECS_PER_DAY;
   time += (Time.Hour * SECS_PER_HOUR);
   time += (Time.Minute * 60);
   time += Time.Second;
   time *= MILLISECS_IN_ONE_SEC;
   time += (Time.Nanosecond / MICROSECS_IN_ONE_SEC);

   return time;
}

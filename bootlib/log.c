/*******************************************************************************
 * Copyright (c) 2008-2011,2014,2020-2021,2023 VMware, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * log.c -- Logging support
 *
 *   Consoles may subscribe to the log system (see log_subscribe()) in order to
 *   be notified each time a new message is logged.
 *
 *   Logs format and severity levels are following the syslog interface:
 *
 *      LOG_EMERG
 *         An assertion in the source code is not verified. If such an error
 *         occurs, both the program and the system are considered as corrupted.
 *         There is no way to recover from such an error.
 *
 *         Or,
 *
 *         An error occurred and has put the system in an unknown state. There
 *         is no way to recover from such an error.
 *
 *      LOG_ALERT
 *          A fatal error occurred but has been caught properly. The program
 *          cannot resume its execution, but can exit or reboot the system.
 *
 *      LOG_CRIT
 *         Other critical error, such as command line syntax error.
 *
 *      LOG_ERR
 *          An error occurred and has been caught properly. The program can
 *          resume its execution.
 *
 *      LOG_WARNING
 *          An unexpected event occurred, that might affect the program
 *          execution. Such events are logged, but they are ignored by the
 *          program itself.
 *
 *      LOG_NOTICE
 *          Normal, but significant, message.
 *
 *      LOG_INFO
 *          Informational message.
 *
 *      LOG_DEBUG
 *          Debug-level message.
 */

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>
#include <boot_services.h>
#include <bootlib.h>
#include <logbuf.h>

#define CONSOLES_MAX_NR       2     /* Framebuffer, serial */
#define LOG_MAX_LEN           1024  /* A single message cannot exceed 1kb */
#define SYSLOG_EMPTY_MSG_SIZE 5     /* STRSIZE("<x>\n") */

typedef struct {
   log_callback_t notify;
   int maxlevel;
} console_t;

static char message[LOG_MAX_LEN];             /* Message buffer */
static console_t consoles[CONSOLES_MAX_NR];

#define SYSLOGBUF_OVERFLOW_MSG "syslog buffer overflow\n"

/* syslog buffer to store logs */
typedef struct syslogbuffer {
   char *buf;
   uint32_t offset;
   uint32_t bufferSize;
   bool expand;
} syslogbuffer;

static syslogbuffer *syslogbuf;

/*-- syslog_get_message_level --------------------------------------------------
 *
 *      Extract the severity level from a syslog message.
 *
 * Parameters
 *      IN  msg:   the message buffer
 *      OUT level: message severity level
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int syslog_get_message_level(const char *msg, int *level)
{
   if (!is_syslog_message(msg)) {
      return ERR_INVALID_PARAMETER;
   }

   *level = msg[1] - '0';

   return ERR_SUCCESS;
}

/*-- syslog_vformat ------------------------------------------------------------
 *
 *      Format a string to a syslog message.
 *
 * Parameters
 *      IN buffer: the message buffer
 *      IN buflen: message buffer size, in bytes
 *      IN level:  message severity (range from 0 to 7)
 *      IN prefix: optional string to prepend to the message
 *      IN msg:    message format string
 *      IN ap:     format parameters list
 *
 * Results
 *      The message length (not including the trailing '\0')
 *----------------------------------------------------------------------------*/
static size_t syslog_vformat(char *msgbuf, size_t buflen, int level,
                             const char *prefix, const char *fmt,
                             va_list ap)
{
   size_t len;
   int n;

   if (buflen < SYSLOG_EMPTY_MSG_SIZE || !IS_SYSLOG_LEVEL(level)) {
      return 0;
   }

   n = snprintf(msgbuf, buflen, "<%c>%s", '0' + (char)level,
                ((prefix != NULL) ? prefix : ""));
   if (n == -1) {
      return 0;
   }
   len = MIN((size_t)n, buflen - 1);

   if (len + 1 <= buflen) {
      n = vsnprintf(msgbuf + len, buflen - len, fmt, ap);
      if (n == -1) {
         return 0;
      }
      len += MIN((size_t)n, buflen - len - 1);
   }

   if (msgbuf[len - 1] != '\n') {
      if ((len + 1) == buflen) {
         len--;
      }
      msgbuf[len++] = '\n';
      msgbuf[len] = '\0';
   }

   return len;
}

/*-- syslogbuf_expand ---------------------------------------------------------
 *
 *      Expand the log buffer capacity.
 *
 * Parameters
 *      None.
 *----------------------------------------------------------------------------*/
static int syslogbuf_expand(uint32_t size)
{
   char *tmp = syslogbuf->buf;

   if (syslogbuf == NULL || !syslogbuf->expand) {
      return ERR_UNSUPPORTED;
   }

   syslogbuf->buf = sys_realloc(syslogbuf->buf, syslogbuf->bufferSize,
                     syslogbuf->bufferSize + size);
   if (syslogbuf->buf == NULL) {
      syslogbuf->buf = tmp;
      return ERR_OUT_OF_RESOURCES;
   }

   memset(syslogbuf->buf + syslogbuf->bufferSize, '\0', size);
   syslogbuf->bufferSize += size;

   return ERR_SUCCESS;
}

/*-- Log -----------------------------------------------------------------------
 *
 *      Send a log message to the registered consoles.
 *
 * Parameters
 *      IN level: message severity (range from 0 to 7)
 *      IN fmt:   message format string
 *      IN ...:   format arguments
 *----------------------------------------------------------------------------*/
void Log(int level, const char *fmt, ...)
{
   size_t size;
   va_list ap;
   int i;

   if (!IS_SYSLOG_LEVEL(level)) {
      level = LOG_DEBUG;
   }

   va_start(ap, fmt);
   size = syslog_vformat(message, LOG_MAX_LEN, level, NULL, fmt, ap) + 1;
   va_end(ap);
   if (size <= 1) {
      return;
   }

   for (i = 0; i < CONSOLES_MAX_NR; i++) {
      if (consoles[i].notify != NULL && level <= consoles[i].maxlevel) {
         consoles[i].notify(message);
      }
   }

   /* Copy log into log buffer */
   if (syslogbuf != NULL && syslogbuf->buf != NULL) {
      uint32_t offset = syslogbuf->offset;

      while (size + strlen(SYSLOGBUF_OVERFLOW_MSG) >
             syslogbuf->bufferSize  - offset) {
         if (syslogbuf_expand(PAGE_SIZE) != ERR_SUCCESS) {
            /* nul terminating is not required in overflow message */
            if (offset + strlen(SYSLOGBUF_OVERFLOW_MSG) <=
                syslogbuf->bufferSize) {
               memcpy(syslogbuf->buf + offset, SYSLOGBUF_OVERFLOW_MSG,
                      strlen(SYSLOGBUF_OVERFLOW_MSG));
               syslogbuf->expand = false;
               syslogbuf->offset = syslogbuf->bufferSize - 1;
            }
            return;
         }
      }

      memcpy(syslogbuf->buf + offset, message, size);
      syslogbuf->offset += size - 1;
   }
}

/*-- log_buffer_info -----------------------------------------------------------
 *
 *      Get the log buffer base address and total buffer size allocated.
 *
 * Results
 *      A pointer to the log buffer and buffer size is updated.
 *----------------------------------------------------------------------------*/
char *log_buffer_info(uint32_t *bufferSize)
{
   if (syslogbuf != NULL) {
      *bufferSize = syslogbuf->bufferSize;
      return syslogbuf->buf;
   }

   return NULL;
}

/*-- log_subscribe -------------------------------------------------------------
 *
 *      Register a console to be notified each time a message is logged.
 *
 * Parameters
 *      IN callback: console routine to be called when a message is logged
 *      IN maxlevel: notify the console only when the message severity level is
 *                   lesser than or equal to this value
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int log_subscribe(log_callback_t callback, int maxlevel)
{
   int i;

   if (!IS_SYSLOG_LEVEL(maxlevel)) {
      maxlevel = LOG_DEBUG;
   }

   log_unsubscribe(callback);
   for (i = 0; i < CONSOLES_MAX_NR; i++) {
      if (consoles[i].notify == NULL) {
         consoles[i].notify = callback;
         consoles[i].maxlevel = maxlevel;
         return ERR_SUCCESS;
      }
   }

   return ERR_OUT_OF_RESOURCES;
}

/*-- log_unsubscribe -----------------------------------------------------------
 *
 *      Remove a console callback from the console table.
 *
 * Parameters
 *      IN callback: console callback to remove
 *----------------------------------------------------------------------------*/
void log_unsubscribe(log_callback_t callback)
{
   int i;

   for (i = 0; i < CONSOLES_MAX_NR; i++) {
      if (consoles[i].notify == callback) {
         consoles[i].notify = NULL;
         consoles[i].maxlevel = 0;
         return;
      }
   }
}

/*-- log_init ------------------------------------------------------------------
 *
 *      Initialize the logging system, initially directing logs to firmware_print.
 *      Until log_init is called, Log() is a no-op.
 *
 *      It is harmless to call this function repeatedly to change the verbosity
 *      level at which firmware_print is registered, but do not call it after
 *      firmware_print is no longer safe.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int log_init(bool verbose)
{
   int status;

   status = log_subscribe(firmware_print, verbose ? LOG_DEBUG : LOG_INFO);
   if (status != ERR_SUCCESS) {
      return status;
   }

#ifndef __COM32__
   /*
    * If a log buffer was already allocated by another bootloader app,
    * find it via the UEFI log buffer protocol.
    */
   if (syslogbuf == NULL) {
      status = logbuf_proto_get(&syslogbuf);
      if (status == ERR_SUCCESS) {
         Log(LOG_DEBUG, "Using existing log buffer protocol");
      }
   }
#endif
   /*
    * Allocate the log buffer if not already allocated
    */
   if (syslogbuf == NULL) {
      syslogbuf = sys_malloc(sizeof *syslogbuf);
      syslogbuf->buf = sys_malloc(PAGE_SIZE);
      if (syslogbuf->buf != NULL) {
         memset(syslogbuf->buf, '\0', PAGE_SIZE);
         syslogbuf->bufferSize = PAGE_SIZE;
         syslogbuf->offset = 0;
         syslogbuf->expand = true;
#ifndef __COM32__
         Log(LOG_DEBUG, "Installing log buffer protocol");
         logbuf_proto_init(syslogbuf);
#endif
      }
   }

   return ERR_SUCCESS;
}

/*-- syslogbuf_expand_disable -------------------------------------------------
 *
 *      Disables syslog buffer expansion, After this new allocations to buffer
 *      will be stopped.
 *
 * Results
 *      None.
 *----------------------------------------------------------------------------*/
void syslogbuf_expand_disable(uint32_t expand_size)
{
   if (syslogbuf != NULL && expand_size != 0) {
      syslogbuf_expand(expand_size);
   }

   syslogbuf->expand = false;
}

/*-- log_data ------------------------------------------------------------------
 *
 *      Log data bytes in hex, 32 bytes per line.
 *
 * Parameters
 *      IN level:    Log level
 *      IN data:     Data to log
 *      IN length:   Length of data
 *----------------------------------------------------------------------------*/
void log_data(int level, uint8_t *data, size_t length)
{
   size_t i, j;
   char buf[65];

   for (i = 0; i < length; ) {
      for (j = 0; j < 32 && i < length; j++, i++) {
         snprintf(&buf[j * 2], 3, "%02x", data[i]);
      }
      Log(level, "%s", buf);
   }
}

/*******************************************************************************
 * Copyright (c) 2008-2011,2015 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * printf.c -- Basic printf() like functions
 *
 *      Only the most common options are implemented here, with no absolute
 *      guarantee that they match the exact standard printf() output.
 *
 *      Warning: No check is performed for ensuring valid options combinations.
 *      Bad options usage such as '%lls' or usage of non-existing options result
 *      in undefined behaviors.
 *
 *   Supported options
 *
 *      Field width
 *         '%' followed with a decimal value string specifies a minimum field
 *         width. If needed, the field is padded on the left with spaces (or
 *         zeros when the decimal value string starts with character '0').
 *
 *      Length modifier (only valid for d, i, o, u, x or X conversions)
 *         %l    : following value is a 'long'
 *         %ll   : following value is a 'long long'
 *         %z    : following value is a 'size_t'
 *
 *      Conversion specifier
 *         %%    : '%' character printing
 *         %c    : character printing
 *         %s    : string printing
 *         %p %P : pointer address pretty printing (lower/upper case)
 *         %d %i : signed value, decimal printing
 *         %u    : unsigned value, decimal printing
 *         %o    : unsigned value, octal printing
 *         %x %X : unsigned value, hexadecimal printing (lower/upper case)
 */

#include <compat.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/types.h>
#include <stdlib.h>

#define PRINTF_FLAG_UPPER        (1 << 0)
#define PRINTF_FLAG_ALT          (1 << 1)
#define PRINTF_FLAG_SIGNED       (1 << 2)
#define PRINTF_FLAG_ZERO_PADDING (1 << 3)

typedef enum {
   PRINTF_TYPE_INT,        /* Default */
   PRINTF_TYPE_LONG,       /* %l */
   PRINTF_TYPE_LONG_LONG,  /* %ll */
   PRINTF_TYPE_SIZE_T,     /* %z */
   PRINTF_TYPE_VOID_P      /* %p, %P */
} printf_type_t;

static INLINE void print_c(char *buffer, size_t buflen, char c, size_t *offset)
{
   if (buflen > 1 && *offset < buflen - 1) {
      buffer[*offset] = c;
   }

   (*offset)++;
}

static INLINE void print_s(char *buffer, size_t buflen, char *s, size_t *offset)
{
   if (s == NULL) {
      s = (char *)"(null)";
   }
   for ( ; *s != '\0'; s++) {
      print_c(buffer, buflen, *s, offset);
   }
}

/*-- parse_padding -------------------------------------------------------------
 *
 *      Parse the optional decimal digit string specifying a minimum field
 *      width. If the converted value has fewer characters than the field width,
 *      it will be padded on the left with spaces or zeros.
 *
 * Parameters
 *      IN  format:  pointer to the format string
 *      OUT flags:   padding flags to be OR'ed with the current printf flags
 *      OUT padding: the field width value
 *
 * Results
 *      The number of characters read from the format string.
 *----------------------------------------------------------------------------*/
static int parse_padding(const char *format, uint32_t *flags, size_t *padding)
{
   size_t width = 0;
   uint32_t fl = 0;
   int len;

   for (len = 0; format[len] >= '0' && format[len] <= '9'; len++) {
      if (len == 0 && format[0] == '0') {
         fl = PRINTF_FLAG_ZERO_PADDING;
         continue;
      }
      width = 10 * width + (format[len] - '0');
   }

   *flags = fl;
   *padding = width;

   return len;
}

/*-- parse_length_modifier -----------------------------------------------------
 *
 *      Parse the optional length modifier for the following d, i, o, u, x or X
 *      conversion.
 *
 * Parameters
 *      IN  format: pointer to the format string
 *      OUT type:   value length type
 *
 * Results
 *      The number of characters read from the format string.
 *----------------------------------------------------------------------------*/
static int parse_length_modifier(const char *format, printf_type_t *type)
{
   switch (format[0]) {
      case 'l':
         if (format[1] == 'l') {
            *type = PRINTF_TYPE_LONG_LONG;
            return 2;
         } else {
            *type = PRINTF_TYPE_LONG;
            return 1;
         }
         break;
      case 'z':
         *type = PRINTF_TYPE_SIZE_T;
         return 1;
      default:
         *type = PRINTF_TYPE_INT;
         return 0;
   }
}

/*-- print_i -------------------------------------------------------------------
 *
 *      Write a printf-formatted value into the given buffer
 *
 * Parameters
 *      IN  buffer:  pointer to the output buffer
 *      IN  buflen:  output buffer size in bytes
 *      IN  value:   the value to convert
 *      IN  base:    numeric base to convert the value to
 *      IN  flags:   printf flags
 *      IN  padding: minimum field width, in number of characters
 *      IN  offset:  position within the buffer where to write the integer
 *      OUT offset:  updated position into buffer
 *
 * Results
 *      The number of characters which would have been written to the output
 *      buffer if enough space had been available.
 *----------------------------------------------------------------------------*/
static int print_i(char *buffer, size_t buflen, uintmax_t value, uint32_t base,
                   uint32_t flags, size_t padding, size_t *offset)
{
   static const char ldigits[] = "0123456789abcdef";
   static const char udigits[] = "0123456789ABCDEF";
   const char *glyphs = (flags & PRINTF_FLAG_UPPER) ? udigits : ldigits;
   uintmax_t val;
   size_t len, pos, ndigits = 0;
   bool negative;

   negative = (flags & PRINTF_FLAG_SIGNED) && ((intmax_t)value < 0);
   if (negative) {
      value = (uintmax_t)(-(intmax_t)value);
   }

   val = value;
   do {
      ndigits++;
      val /= base;
   } while (val > 0);

   len = ndigits + (negative ? 1 : 0)
       + (((flags & PRINTF_FLAG_ALT) && base == 16) ? 2 : 0);
   padding = (padding > len) ? padding - len : 0;
   len += padding;

   /* Space padding */
   if (!(flags & PRINTF_FLAG_ZERO_PADDING)) {
      for ( ; padding > 0; padding--) {
         print_c(buffer, buflen, ' ', offset);
      }
   }

   /* '0x' or '0X' hex prefix */
   if ((flags & PRINTF_FLAG_ALT) && base == 16) {
      print_c(buffer, buflen, '0', offset);
      print_c(buffer, buflen, (flags & PRINTF_FLAG_UPPER) ? 'X' : 'x', offset);
   }

   /* '-' negative prefix */
   if (negative) {
      print_c(buffer, buflen, '-', offset);
   }

   /* Zero padding */
   while (padding-- > 0) {
      print_c(buffer, buflen, '0', offset);
   }

   /* Write actual number starting from the right */
   pos = *offset + ndigits - 1;
   do {
      print_c(buffer, buflen, glyphs[value % base], &pos);
      value /= base;
      pos -= 2;
   } while (value != 0);

   (*offset) += ndigits;

   return (int)len;
}

/*-- vsnprintf -----------------------------------------------------------------
 *
 *      Basic vsnprintf() function.
 *
 * Parameters
 *      IN buffer: pointer to the output buffer
 *      IN buflen: output buffer size in bytes
 *      IN format: printf-styled format string
 *      IN ap:     list of arguments for the format string
 *
 * Results
 *      The number of characters which would have been written to the output
 *      buffer if enough space had been available, or -1 if an error occurred.
 *----------------------------------------------------------------------------*/
int vsnprintf(char *buffer, size_t buflen, const char *format, va_list ap)
{
   size_t padding, offset;
   printf_type_t type;
   uint32_t base, flags;
   uintmax_t u;

   for (offset = 0; *format != '\0'; format++) {
      if (*format == '%') {
         format++;
         format += parse_padding(format, &flags, &padding);
         format += parse_length_modifier(format, &type);

         switch (*format) {
            case '%':
               print_c(buffer, buflen, '%', &offset);
               break;
            case 'c':
               print_c(buffer, buflen, (char)va_arg(ap, int), &offset);
               break;
            case 's':
               print_s(buffer, buflen, (char *)va_arg(ap, char *), &offset);
               break;
            case 'P':
               flags |= PRINTF_FLAG_UPPER;
            case 'p':
               flags |= PRINTF_FLAG_ALT;
               type = PRINTF_TYPE_VOID_P;
               base = 16;
               goto vprint_i;
            case 'd':
            case 'i':
               flags |= PRINTF_FLAG_SIGNED;
               base = 10;
               goto vprint_i;
            case 'o':
               base = 8;
               goto vprint_i;
            case 'u':
               base = 10;
               goto vprint_i;
            case 'X':
               flags |= PRINTF_FLAG_UPPER;
            case 'x':
               base = 16;
               goto vprint_i;
            default:
               return -1;

           vprint_i:
               switch (type) {
                  case PRINTF_TYPE_LONG:
                     u = (uintmax_t)va_arg(ap, unsigned long int);
                     break;
                  case PRINTF_TYPE_LONG_LONG:
                     u = (uintmax_t)va_arg(ap, unsigned long long int);
                     break;
                  case PRINTF_TYPE_SIZE_T:
                     u = (uintmax_t)va_arg(ap, size_t);
                     break;
                  case PRINTF_TYPE_VOID_P:
                     u = (uintmax_t)(uintptr_t)va_arg(ap, void *);
                     break;
                  case PRINTF_TYPE_INT:
                  default:
                     u = (uintmax_t)va_arg(ap, unsigned int);
                     break;
               }
               print_i(buffer, buflen, u, base, flags, padding, &offset);
         }
      } else {
         print_c(buffer, buflen, *format, &offset);
      }
   }

   if (buflen > 0) {
      buffer[(offset < buflen) ? offset : buflen - 1] = '\0';
   }

   return (int)offset;
}

/*-- snprintf ------------------------------------------------------------------
 *
 *      Write at most 'size' bytes (including the trailing '\0') to 'str'.
 *
 * Parameters
 *      IN str:    pointer to the ouptut buffer
 *      IN size:   maximum number of bytes to write
 *      IN format: printf-styled format string
 *      IN ...:    list of arguments for the format string
 *
 * Results
 *      The number of characters which would have been written to the output
 *      buffer if enough space had been available, or -1 if an error occurred.
 *----------------------------------------------------------------------------*/
int snprintf(char *str, size_t size, const char *format, ...)
{
   va_list ap;
   int status;

   va_start(ap, format);
   status = vsnprintf(str, size, format, ap);
   va_end(ap);

   return status;
}

/*-- asprintf ------------------------------------------------------------------
 *
 *      Analog of sprintf(), except that it allocates a string large enough to
 *      hold the output including the trailing '\0' and returns a pointer to it
 *      via the first argument.
 *
 * Parameters
 *      OUT str:    pointer to the freshly allocated output string
 *      IN  format: printf-styled format string
 *      IN  ...:    list of arguments for the format string
 *
 * Results
 *      The number of characters written in the allocated buffer (not including
 *      the trailing '\0', or -1 if an error occurred.
 *----------------------------------------------------------------------------*/
int asprintf(char **str, const char *format, ...)
{
   char *buffer;
   va_list ap;
   int len;

   va_start(ap, format);
   len = vsnprintf(NULL, 0, format, ap);
   va_end(ap);

   if (len < 0) {
      return -1;
   }

   buffer = malloc(len + 1);
   if (buffer == NULL) {
      return -1;
   }

   va_start(ap, format);
   len = vsnprintf(buffer, len + 1, format, ap);
   va_end(ap);

   if (len != -1) {
      *str = buffer;
   }

   return len;
}

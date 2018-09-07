/*------------------------------------------------------------------------------
 *   Copyright 2004-2009 H. Peter Anvin - All Rights Reserved
 *   Portions Copyright 2009 Intel Corporation; author: H. Peter Anvin
 *
 *   Permission is hereby granted, free of charge, to any person obtaining a
 *   copy of this software and associated documentation files (the "Software"),
 *   to deal in the Software without restriction, including without limitation
 *   the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *   and/or sell copies of the Software, and to permit persons to whom the
 *   Software is furnished to do so, subject to the following conditions:
 *
 *   The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *   DEALINGS IN THE SOFTWARE.
 *----------------------------------------------------------------------------*/

/*
 * getopt.c -- Basic getopt()
 */

#include <compat.h>
#include <sys/types.h>
#include <string.h>

char *optarg;
int optopt;

/*
 * The variable optind is the index of the next element to be processed in argv.
 * This value is initialized to 1. The caller can reset it to 1 to restart
 * scanning of the same argv, or when scanning a new argument vector.
 */
int optind = 1;

/*-- getopt --------------------------------------------------------------------
 *
 *      The getopt() function parses the command-line arguments. An element of
 *      argv that starts with '-' (and is not exactly "-" or "--") is an option
 *      element.The characters of this element (aside from the initial '-') are
 *      option characters. If getopt() is called repeatedly, it returns
 *      successively each of the option characters from each of the option
 *      elements.
 *
 *      If getopt() finds another option character, it returns that character,
 *      updating the external variable optind and a static variable nextchar so
 *      that the next call to getopt() can resume the scan with the following
 *      option character or argv-element.
 *
 *      optstring contains the legitimate option characters. If such a character
 *      is followed by a colon, the option requires an argument, so getopt()
 *      places a pointer to the following text in the same argv-element, or the
 *      text of the following argv-element, in optarg.
 *
 *      The special argument "--" forces an end of option-scanning.
 *
 * Parameters
 *      IN argc:      number of command line arguments
 *      IN argv:      array of arguments
 *      IN optstring: string containing the legitimate option characters
 *
 * Results
 *      (-1) if there are no more option characters. Then optind is the index in
 *      argv of the first argv-element that is not an option.
 *
 *      If getopt() finds an option character in argv that was not included in
 *      optstring, or if it detects a missing option argument, it returns '?'
 *      and sets the external variable optopt to the actual option character. If
 *      the first character of optstring is a colon (':'), then getopt() returns
 *      ':' instead of '?' to indicate a missing option argument.
 *----------------------------------------------------------------------------*/
int getopt(int argc, char *const *argv, const char *optstring)
{
   static const char *nextchar = NULL;
   const char *option, *p;
   int opt;

   if (optind >= argc) {
      return -1;
   }

   option = argv[optind];

   if (option == NULL || option[0] != '-' || option[1] == '\0') {
      return -1;
   } else if (option[1] == '-' && option[2] == '\0') {
      optind++;
      return -1;
   }

   if (!(nextchar >= option && nextchar <= option + strlen(option))) {
      nextchar = &option[1];
   }
   opt = *nextchar++;

   if (opt != ':') {
      p = strchr(optstring, opt);
      if (p != NULL) {
         if (p[1] == ':') {
            if (*nextchar != '\0') {
               optarg = (char *)nextchar;
               optind++;
            } else {
               if (optind + 1 < argc && argv[optind + 1] != NULL) {
                  optarg = (char *)argv[optind + 1];
                  optind += 2;
               } else {
                  optopt = opt;
                  return (optstring[0] == ':') ? ':' : '?';
               }
            }
         } else if (*nextchar == '\0') {
            optind++;
         }

         return opt;
      }
   }

   if (*nextchar == '\0') {
      optind++;
   }

   optopt = opt;
   return '?';
}

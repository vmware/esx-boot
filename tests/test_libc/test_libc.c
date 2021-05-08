/*******************************************************************************
 * Copyright (c) 2020 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * test_libc.c -- sanity tests for some libc code.
 *
 *   test_libc [-t]
 *
 *      OPTIONS
 *         -t <testname>  Run a specific test.
 *
 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/types.h>
#include <bootlib.h>
#include <boot_services.h>

#define TESTS   \
   TEST(strnlen)\
   TEST(strtoul)

typedef struct {
   const char *name;
   bool (*fn)(void);
} test_entry;

/*-- strnlen_test --------------------------------------------------------------
 *
 *      Sanity checks on strnlen.
 *
 * Results
 *      True if failed.
 *----------------------------------------------------------------------------*/
static bool strnlen_test(void)
{
   unsigned i;
   bool failed;
   const char *str = "Hello!";
   unsigned len = strlen(str);
   unsigned runs[] = {
      /* maxlen -> result */
      len, len,
      len - 1, len - 1,
      len + 1, len,
      len + 100, len,
      0, 0,
   };

   for (i = 0, failed = false; i < ARRAYSIZE(runs); i += 2) {
      unsigned res = strnlen(str, runs[i]);
      if (res != runs[i + 1]) {
         Log(LOG_ERR, "Case %u: strnlen(%s, %u) %u != %u", i, str, res,
             runs[i], runs[i + 1]);
         failed = true;
      }
   }

   return failed;
}

/*-- strtoul_test --------------------------------------------------------------
 *
 *      Some sanity checks on strnlen.
 *
 * Results
 *      True if failed.
 *----------------------------------------------------------------------------*/
static bool strtoul_test(void)
{
   unsigned i;
   bool failed;

#if defined(only_em64t) || defined(only_arm64)
#define MAX_ULONG_STR "18446744073709551615"
#else
#define MAX_ULONG_STR "4294967295"
#endif

   struct {
      const char *s;
      unsigned base;
      unsigned long p;
      bool all_valid;
   } runs[] = {
      /*  0 */ { "0", 10, 0, true },
      /*  1 */ { "", 10, 0, false },
      /*  2 */ { "1337", 10, 1337, true },
      /*  3 */ { "-1337", 10, (unsigned long) (-1337), true },
      /*  4 */ {     MAX_ULONG_STR        , 10, ULONG_MAX, true },
      /*  5 */ {     MAX_ULONG_STR "1"    , 10, ULONG_MAX, true },
      /*  6 */ { "-" MAX_ULONG_STR "1"    , 10, ULONG_MAX, true },
      /*  7 */ {     MAX_ULONG_STR     "A", 10, ULONG_MAX, false },
      /*  8 */ { "-" MAX_ULONG_STR        , 10, (unsigned long) (-ULONG_MAX), true },
      /*  9 */ { "-" MAX_ULONG_STR     "A", 10, (unsigned long) (-ULONG_MAX), false },
      /* 10 */ { "-" MAX_ULONG_STR "1" "A", 10, ULONG_MAX, false },
   };

   for (i = 0, failed = false; i < ARRAYSIZE(runs); i++) {
      char *endptr;
      bool all_valid;
      unsigned long p = strtoul(runs[i].s, &endptr, runs[i].base);
      all_valid = (*endptr == '\0' && endptr != runs[i].s);

      if (runs[i].p != p || all_valid != runs[i].all_valid) {
         Log(LOG_ERR, "Case %u: strtoul(%s, &endptr, %u) %lu != %lu (all_valid %u)",
             i, runs[i].s, runs[i].base, p, runs[i].p, all_valid);
         failed = true;
      }
   }

   return failed;
}

#define TEST(x) { #x, x ## _test },
static test_entry tests[] = {
   TESTS
};
#undef TEST


/*-- test_libc_init ------------------------------------------------------------
 *
 *      Parse test_libc command line options.
 *
 * Parameters
 *      IN argc: number of command line arguments
 *      IN argv: pointer to the command line arguments array
 *      OUT test: pointer to name of the test to run, if any
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int test_libc_init(int argc, char **argv, char **test)
{
   int opt;
   unsigned i;

   if (argc == 0 || argv == NULL || argv[0] == NULL) {
      return ERR_INVALID_PARAMETER;
   }

   if (argc > 1) {
      optind = 1;
      do {
         opt = getopt(argc, argv, "t:h");
         switch (opt) {
            case -1:
               break;
            case 't':
               *test = optarg;
               break;
            case 'h':
            case '?':
            default:
               Log(LOG_ERR, "Usage: %s [-t test]", argv[0]);
               Log(LOG_ERR, "Available tests:");
               for (i = 0; i < ARRAYSIZE(tests); i++) {
                  Log(LOG_ERR, " - %s", tests[i].name);
               }
               return ERR_SYNTAX;
         }
      } while (opt != -1);
   }

   return ERR_SUCCESS;
}

/*-- main ----------------------------------------------------------------------
 *
 *      test_libc main function.
 *
 * Parameters
 *      IN argc: number of command line arguments
 *      IN argv: pointer to the command line arguments array
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int main(int argc, char **argv)
{
   int status;
   unsigned i;
   bool failed;
   bool tests_ran;
   char *test = NULL;

   status = log_init(true);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = test_libc_init(argc, argv, &test);
   if (status != ERR_SUCCESS) {
      return status;
   }

   for (i = 0, tests_ran = false, failed = false; i < ARRAYSIZE(tests); i++) {
      test_entry *t = &tests[i];
      if (test != NULL && strcmp(test, t->name) != 0) {
         continue;
      }
      Log(LOG_ERR, "Checking %s", t->name);
      failed |= t->fn();
      tests_ran |= true;
   }

   if (failed) {
      return ERR_TEST_FAILURE;
   }

   if (!tests_ran) {
      Log(LOG_ERR, "Invalid test specified - no tests ran");
      return ERR_INVALID_PARAMETER;
   }

   Log(LOG_ERR, "All tests passed");
   return ERR_SUCCESS;
}

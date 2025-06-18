/*******************************************************************************
 * Copyright (c) 2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * urlresolve.c -- Resolve relative URLs, per RFC 3986.
 */

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

#if defined(TEST_URLRESOLVE)
#include <stdio.h>
bool debug = false;
#define ULOG(...) if (debug) printf(__VA_ARGS__)
#else
#define ULOG(...)
#endif //TEST_URLRESOLVE

typedef struct str_with_len {
   const char *s;
   int l;
} str_with_len;

typedef struct url_parsed {
   str_with_len scheme;
   str_with_len authority;
   str_with_len path;
   str_with_len query;
   str_with_len fragment;
} url_parsed;

/*-- url_parse -----------------------------------------------------------------
 *
 *      Parse an absolute or relative URL into its five components: scheme,
 *      authority, path, query, fragment.  Rather than copying or modifying the
 *      URL in place, five (pointer, length) pairs are returned.  See RFC 3986,
 *      section 3.
 *
 *      In the output, if a field is present, it includes its delimiter; if
 *      absent, it is zero length with no delimiter.  Specifically, if present:
 *
 *      - scheme includes the trailing ":"
 *      - authority includes the leading "//"
 *      - path includes the leading "/" if present
 *      - query includes the leading "?"
 *      - fragment includes the leading "#"
 *
 *      IN url: URL to parse
 *      OUT o:  Output of parse
 *----------------------------------------------------------------------------*/
static void url_parse(const char *url, url_parsed *o)
{
   const char *p, *q;

   memset(o, 0, sizeof(*o));

   /* Parse out scheme */
   p = q = url;
   if (isalpha((unsigned char)*q)) {
      while (isalpha((unsigned char)*q) || isdigit((unsigned char)*q) ||
             *q == '+' || *q == '.' || *q == '-') {
         q++;
      }
      if (*q == ':') {
         q++;
         o->scheme.s = p;
         o->scheme.l = q - p;
         ULOG("p1 %.*s\n", o->scheme.l, o->scheme.s);
         p = q;
      }
   }

   /* Parse out authority */
   if (p[0] == '/' && p[1] == '/') {
      ULOG("p2\n");
      o->authority.s = p;
      q = p + 2;
      while (*q != '/' && *q != '?' && *q != '#' && *q != '\0') {
         q++;
      }
      o->authority.l = q - p;
      p = q;
   }

   /* Parse out path */
   o->path.s = p;
   q = p;
   while (*q != '?' && *q != '#' && *q != '\0') {
      q++;
   }
   o->path.l = q - p;
   p = q;

   /* Parse out query */
   if (p[0] == '?') {
      ULOG("p3\n");
      o->query.s = p;
      q = p;
      while (*q != '#' && *q != '\0') {
         q++;
      }
      o->query.l = q - p;
      p = q;
   }

   /* Parse out fragment */
   if (p[0] == '#') {
      ULOG("p4\n");
      o->fragment.s = p;
      q = p;
      while (*q != '\0') {
         q++;
      }
      o->fragment.l = q - p;
   }
   ULOG("p5 %s -> s=\"%.*s\" a=\"%.*s\" p=\"%.*s\" q=\"%.*s\" f=\"%.*s\"\n",
        url, o->scheme.l, o->scheme.s, o->authority.l, o->authority.s,
        o->path.l, o->path.s, o->query.l, o->query.s,
        o->fragment.l, o->fragment.s);
}

/*-- url_remove_dot_segments ---------------------------------------------------
 *
 *      Remove invalid or extraneous "." and ".." segments, per RFC 3986
 *      section 5.2.4.
 *
 * Parameters
 *      IN  path_in:  input path
 *      OUT path_out: output path
 *
 * Results
 *      path_out is a copy of path_in with dot segments removed.  The caller
 *      should free path_out->s when done with it.
 *----------------------------------------------------------------------------*/
static void url_remove_dot_segments(str_with_len path_in,
                                    str_with_len *path_out)
{
   char *inbuf = malloc(path_in.l + 1);
   char *outbuf = malloc(path_in.l + 1);
   char *inp, *outp;

   memcpy(inbuf, path_in.s, path_in.l);
   inbuf[path_in.l] = '\0';
   inp = inbuf;
   outp = outbuf;
   ULOG("d0 %s\n", inbuf);
   while (*inp != '\0') {
      bool dotdot = false;

      if (strncmp(inp, "../", 3) == 0) {          // RFC 3986 sec 5.2.4 step 2A
         ULOG("d1\n");
         inp += 3;
      } else if (strncmp(inp, "./", 2) == 0) {    // 2A
         ULOG("d2\n");
         inp += 2;
      } else if (strncmp(inp, "/./", 3) == 0) {   // 2B
         ULOG("d3\n");
         inp += 2;
      } else if (strcmp(inp, "/.") == 0) {        // 2B
         ULOG("d4\n");
         inp++;
         *inp = '/';
      } else if (strncmp(inp, "/../", 4) == 0) {  // 2C
         ULOG("d5\n");
         inp += 3;
         dotdot = true;
      } else if (strcmp(inp, "/..") == 0) {       // 2C
         ULOG("d6\n");
         inp += 2;
         *inp = '/';
         dotdot = true;
      } else if (strcmp(inp, ".") == 0) {         // 2D
         ULOG("d7\n");
         inp++;
      } else if (strcmp(inp, "..") == 0) {        // 2D
         ULOG("d8\n");
         inp += 2;
      } else {                                    // 2E
         ULOG("d9\n");
         do {
            *outp++ = *inp++;
         } while (*inp != '/' && *inp != '\0');
      }
      if (dotdot) {                               // finish 2C
         ULOG("d10\n");
         while (outp > outbuf) {
            outp--;
            if (*outp == '/') {
               break;
            }
         }
      }
   }
   *outp = '\0'; // not really needed
   free(inbuf);
   path_out->s = outbuf;
   path_out->l = outp - outbuf;
}

/*-- url_resolve_relative ------------------------------------------------------
 *
 *      Resolve a possibly relative URL into an absolute one.  See RFC 3986,
 *      section 5.2.
 *
 * Parameters
 *      IN base_url: base URL
 *      IN rel_url:  relative URL
 *
 * Results
 *      Absolute URL.  The caller should free this memory when done with it.
 *----------------------------------------------------------------------------*/
char *url_resolve_relative(const char *base_url,
                           const char *rel_url)
{
   url_parsed r, base, t;
   char *ts, *tp;

   ULOG("r0 base=\"%s\" rel=\"%s\"\n", base_url, rel_url);
   url_parse(rel_url, &r);
   url_parse(base_url, &base);

   if (r.scheme.l != 0) {
      ULOG("r1\n");
      t.scheme = r.scheme;
      t.authority = r.authority;
      url_remove_dot_segments(r.path, &t.path);
      t.query = r.query;
   } else {
      if (r.authority.l != 0) {
         ULOG("r2\n");
         t.authority = r.authority;
         url_remove_dot_segments(r.path, &t.path);
         t.query = r.query;
      } else {
         ULOG("r3\n");
         if (r.path.l == 0) {
            ULOG("r4\n");
            t.path = base.path;
            if (r.query.l != 0) {
               ULOG("r5\n");
               t.query = r.query;
            } else {
               ULOG("r6\n");
               t.query = base.query;
            }
         } else {
            ULOG("r7\n");
            if (r.path.s[0] == '/') {
               ULOG("r8\n");
               url_remove_dot_segments(r.path, &t.path);
            } else {
               // Merge paths (5.2.3)
               str_with_len m;
               char *ms;
               ULOG("r9\n");
               ms = malloc(base.path.l + r.path.l + 1);
               if (base.authority.l != 0 && base.path.l == 0) {
                  ULOG("r10\n");
                  ms[0] = '/';
                  memcpy(ms + 1, r.path.s, r.path.l);
                  m.s = ms;
                  m.l = r.path.l + 1;
               } else {
                  const char *p;
                  ULOG("r11\n");
                  p = base.path.s + base.path.l;
                  while (p > base.path.s) {
                     if (*--p == '/') {
                        p++;
                        break;
                     }
                  }
                  memcpy(ms, base.path.s, p - base.path.s);
                  memcpy(ms + (p - base.path.s), r.path.s, r.path.l);
                  m.s = ms;
                  m.l = p - base.path.s + r.path.l;
               }
               ULOG("r12 %.*s\n", m.l, m.s);
               url_remove_dot_segments(m, &t.path);
               free(ms);
               ULOG("r13 %.*s\n", t.path.l, t.path.s);
            }
            t.query = r.query;
         }
         t.authority = base.authority;
      }
      t.scheme = base.scheme;
   }
   t.fragment = r.fragment;

   ts = malloc(t.scheme.l + t.authority.l + t.path.l + t.query.l +
               t.fragment.l + 1);
   tp = ts;
   memcpy(tp, t.scheme.s, t.scheme.l);
   tp += t.scheme.l;
   memcpy(tp, t.authority.s, t.authority.l);
   tp += t.authority.l;
   memcpy(tp, t.path.s, t.path.l);
   tp += t.path.l;
   memcpy(tp, t.query.s, t.query.l);
   tp += t.query.l;
   memcpy(tp, t.fragment.s, t.fragment.l);
   tp += t.fragment.l;
   *tp = '\0';

   ULOG("r14 s=\"%.*s\" a=\"%.*s\" p=\"%.*s\" q=\"%.*s\" f=\"%.*s\" -> %s\n",
        t.scheme.l, t.scheme.s, t.authority.l, t.authority.s,
        t.path.l, t.path.s, t.query.l, t.query.s,
        t.fragment.l, t.fragment.s, ts);

   if (t.path.s != base.path.s) {
      /*
       * t.path.s is always either base.path.s or allocated by
       *  url_remove_dot_segments.
       */
      free((void *)t.path.s);
   }

   return ts;
}

#ifdef TEST_URLRESOLVE
/*
 * Regression test app.
 *
 * To compile:
 *   cc -o urlresolve -DTEST_URLRESOLVE urlresolve.c
 *
 * Usage:
 *   ./urlresolve [-d] [base rel]
 *
 * Without base and rel, check that all the wired-in examples give the expected
 * result.  With base and rel, print url_resolve_relative(base, rel).  With -d,
 * also print verbose debug output.
 */
#include <stdio.h>

typedef struct Example {
   const char *base;
   const char *rel;
   const char *expected;
} Example;

Example examples[] = {
   /* base                  rel              expected */

   /* RFC 3986 section 5.4.1: Normal Examples */
   { "http://a/b/c/d;p?q", "g:h",           "g:h" },
   { "http://a/b/c/d;p?q", "g",             "http://a/b/c/g" },
   { "http://a/b/c/d;p?q", "./g",           "http://a/b/c/g" },
   { "http://a/b/c/d;p?q", "g/",            "http://a/b/c/g/" },
   { "http://a/b/c/d;p?q", "/g",            "http://a/g" },
   { "http://a/b/c/d;p?q", "//g",           "http://g" },
   { "http://a/b/c/d;p?q", "?y",            "http://a/b/c/d;p?y" },
   { "http://a/b/c/d;p?q", "g?y",           "http://a/b/c/g?y" },
   { "http://a/b/c/d;p?q", "#s",            "http://a/b/c/d;p?q#s" },
   { "http://a/b/c/d;p?q", "g#s",           "http://a/b/c/g#s" },
   { "http://a/b/c/d;p?q", "g?y#s",         "http://a/b/c/g?y#s" },
   { "http://a/b/c/d;p?q", ";x",            "http://a/b/c/;x" },
   { "http://a/b/c/d;p?q", "g;x",           "http://a/b/c/g;x" },
   { "http://a/b/c/d;p?q", "g;x?y#s",       "http://a/b/c/g;x?y#s" },
   { "http://a/b/c/d;p?q", "",              "http://a/b/c/d;p?q" },
   { "http://a/b/c/d;p?q", ".",             "http://a/b/c/" },
   { "http://a/b/c/d;p?q", "./",            "http://a/b/c/" },
   { "http://a/b/c/d;p?q", "..",            "http://a/b/" },
   { "http://a/b/c/d;p?q", "../",           "http://a/b/" },
   { "http://a/b/c/d;p?q", "../g",          "http://a/b/g" },
   { "http://a/b/c/d;p?q", "../..",         "http://a/" },
   { "http://a/b/c/d;p?q", "../../",        "http://a/" },
   { "http://a/b/c/d;p?q", "../../g",       "http://a/g" },

   /* RFC 3986 section 5.4.2: Abnormal Examples */
   { "http://a/b/c/d;p?q", "../../../g",    "http://a/g" },
   { "http://a/b/c/d;p?q", "../../../../g", "http://a/g" },
   { "http://a/b/c/d;p?q", "/./g",          "http://a/g" },
   { "http://a/b/c/d;p?q", "/../g",         "http://a/g" },
   { "http://a/b/c/d;p?q", "g.",            "http://a/b/c/g." },
   { "http://a/b/c/d;p?q", ".g",            "http://a/b/c/.g" },
   { "http://a/b/c/d;p?q", "g..",           "http://a/b/c/g.." },
   { "http://a/b/c/d;p?q", "..g",           "http://a/b/c/..g" },
   { "http://a/b/c/d;p?q", "./../g",        "http://a/b/g" },
   { "http://a/b/c/d;p?q", "./g/.",         "http://a/b/c/g/" },
   { "http://a/b/c/d;p?q", "g/./h",         "http://a/b/c/g/h" },
   { "http://a/b/c/d;p?q", "g/../h",        "http://a/b/c/h" },
   { "http://a/b/c/d;p?q", "g;x=1/./y",     "http://a/b/c/g;x=1/y" },
   { "http://a/b/c/d;p?q", "g;x=1/../y",    "http://a/b/c/y" },
   { "http://a/b/c/d;p?q", "g?y/./x",       "http://a/b/c/g?y/./x" },
   { "http://a/b/c/d;p?q", "g?y/../x",      "http://a/b/c/g?y/../x" },
   { "http://a/b/c/d;p?q", "g#s/./x",       "http://a/b/c/g#s/./x" },
   { "http://a/b/c/d;p?q", "g#s/../x",      "http://a/b/c/g#s/../x" },

#if true
   /* RFC 3986 section 5.4.2: "For strict parsers" */
   { "http://a/b/c/d;p?q", "http:g",        "http:g" },
#else
   /* Alternative for backward compatibility; see also RFC 3986 section 5.2.2 */
   { "http://a/b/c/d;p?q", "http:g",        "http://a/b/c/g" },
#endif

   /* Additional examples to reach the remaining ULOG() points in the code */
   { "http:",              "../.",          "http:" },            // d1
   { "http:",              "./.",           "http:" },            // d2
   { "http:",              ".#foo",         "http:#foo" },        // d7
   { "http:",              "..#foo",        "http:#foo" },        // d8
   { "http://ralph",       "foo",           "http://ralph/foo" }, // r10

   /* Additional case discussed in code review: relative URL with leading
    * colon.  This is not a legal URL per RFC 3986.  Our behavior conforms to
    * https://url.spec.whatwg.org/#concept-basic-url-parser as last updated
    * 25 Mar 2024, as well as Firefox and Chrome (tested 14 Aug 2024). */
   { "http://a/b/c/d;p?q", "://g",          "http://a/b/c/://g" },
};

int
main(int argc, char **argv)
{
   int argp = 1;

   if (argp < argc) {
      if (strcmp(argv[argp], "-d") == 0) {
         debug = 1;
         argp++;
      }
   }

   if (argp == argc) {
      char *out;
      int i, correct = 0, wrong = 0;

      /* Loop through wired-in examples and check results */
      for (i = 0; i < sizeof(examples) / sizeof(Example); i++) {
         Example *ex = &examples[i];
         out = url_resolve_relative(ex->base, ex->rel);
         if (strcmp(out, ex->expected) == 0) {
            correct++;
         } else {
            fprintf(stderr, "ERROR: base=\"%s\" rel=\"%s\" "
                    "expected=\"%s\" got=\"%s\"\n",
                    ex->base, ex->rel, ex->expected, out);
            wrong++;
         }
         free(out);
      }
      printf("%d correct; %d wrong\n", correct, wrong);
      return 0;

   } else if (argp == argc - 2) {
      char *out;

      /* Do one example entered on command line */
      out = url_resolve_relative(argv[argp], argv[argp + 1]);
      printf("%s\n", out);
      return 0;
   }

   fprintf(stderr, "Usage: %s [-d] [base_url rel_url]\n", argv[0]);
   return 99;
}

#endif //TEST_URLRESOLVE

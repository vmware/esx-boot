/*******************************************************************************
 * Copyright (c) 2008-2011 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * ctypes.c -- Character classification
 */

#include <ctype.h>

/*
 * ISO 8859-1 character types
 */
const unsigned char libc_ctype[256] = {
   /* NUL */ C_CTRL,  /* SOH */ C_CTRL,  /* STX */ C_CTRL,  /* ETX */ C_CTRL,
   /* EOT */ C_CTRL,  /* ENQ */ C_CTRL,  /* ACK */ C_CTRL,  /* BEL */ C_CTRL,
   /* BS  */ C_SPACE, /* TAB */ C_SPACE, /* LF  */ C_SPACE, /* VT  */ C_SPACE,
   /* FF  */ C_SPACE, /* CR  */ C_SPACE, /* SO  */ C_CTRL,  /* SI  */ C_CTRL,
   /* DLE */ C_CTRL,  /* DC1 */ C_CTRL,  /* DC2 */ C_CTRL,  /* DC3 */ C_CTRL,
   /* DC4 */ C_CTRL,  /* NAK */ C_CTRL,  /* SYN */ C_CTRL,  /* ETB */ C_CTRL,
   /* CAN */ C_CTRL,  /* EM  */ C_CTRL,  /* SUB */ C_CTRL,  /* ESC */ C_CTRL,
   /* FS  */ C_CTRL,  /* GS  */ C_CTRL,  /* RS  */ C_CTRL,  /* US  */ C_CTRL,
   /* SPC */ C_SPACE, /* '!' */ C_PUNCT, /* '"' */ C_PUNCT, /* '#' */ C_PUNCT,
   /* '$' */ C_PUNCT, /* '%' */ C_PUNCT, /* '&' */ C_PUNCT, /* ''' */ C_PUNCT,
   /* '(' */ C_PUNCT, /* ')' */ C_PUNCT, /* '*' */ C_PUNCT, /* '+' */ C_PUNCT,
   /* ',' */ C_PUNCT, /* '-' */ C_PUNCT, /* '.' */ C_PUNCT, /* '/' */ C_PUNCT,
   /* '0' */ C_DIGIT | C_XDIGIT,         /* '1' */ C_DIGIT | C_XDIGIT,
   /* '2' */ C_DIGIT | C_XDIGIT,         /* '3' */ C_DIGIT | C_XDIGIT,
   /* '4' */ C_DIGIT | C_XDIGIT,         /* '5' */ C_DIGIT | C_XDIGIT,
   /* '6' */ C_DIGIT | C_XDIGIT,         /* '7' */ C_DIGIT | C_XDIGIT,
   /* '8' */ C_DIGIT | C_XDIGIT,         /* '9' */ C_DIGIT | C_XDIGIT,
   /* ':' */ C_PUNCT, /* ';' */ C_PUNCT, /* '<' */ C_PUNCT, /* '=' */ C_PUNCT,
   /* '>' */ C_PUNCT, /* '?' */ C_PUNCT, /* '@' */ C_PUNCT,
   /* 'A' */ C_UPPER | C_XDIGIT,         /* 'B' */ C_UPPER | C_XDIGIT,
   /* 'C' */ C_UPPER | C_XDIGIT,         /* 'D' */ C_UPPER | C_XDIGIT,
   /* 'E' */ C_UPPER | C_XDIGIT,         /* 'F' */ C_UPPER | C_XDIGIT,
   /* 'G' */ C_UPPER, /* 'H' */ C_UPPER, /* 'I' */ C_UPPER, /* 'J' */ C_UPPER,
   /* 'K' */ C_UPPER, /* 'L' */ C_UPPER, /* 'M' */ C_UPPER, /* 'N' */ C_UPPER,
   /* 'O' */ C_UPPER, /* 'P' */ C_UPPER, /* 'Q' */ C_UPPER, /* 'R' */ C_UPPER,
   /* 'S' */ C_UPPER, /* 'T' */ C_UPPER, /* 'U' */ C_UPPER, /* 'V' */ C_UPPER,
   /* 'W' */ C_UPPER, /* 'X' */ C_UPPER, /* 'Y' */ C_UPPER, /* 'Z' */ C_UPPER,
   /* '[' */ C_PUNCT, /* '\' */ C_PUNCT, /* ']' */ C_PUNCT, /* '^' */ C_PUNCT,
   /* '_' */ C_PUNCT, /* '`' */ C_PUNCT,
   /* 'a' */ C_LOWER | C_XDIGIT,         /* 'b' */ C_LOWER | C_XDIGIT,
   /* 'c' */ C_LOWER | C_XDIGIT,         /* 'd' */ C_LOWER | C_XDIGIT,
   /* 'e' */ C_LOWER | C_XDIGIT,         /* 'f' */ C_LOWER | C_XDIGIT,
   /* 'g' */ C_LOWER, /* 'h' */ C_LOWER, /* 'i' */ C_LOWER, /* 'j' */ C_LOWER,
   /* 'k' */ C_LOWER, /* 'l' */ C_LOWER, /* 'm' */ C_LOWER, /* 'n' */ C_LOWER,
   /* 'o '*/ C_LOWER, /* 'p' */ C_LOWER, /* 'q' */ C_LOWER, /* 'r' */ C_LOWER,
   /* 's' */ C_LOWER, /* 't' */ C_LOWER, /* 'u' */ C_LOWER, /* 'v' */ C_LOWER,
   /* 'w' */ C_LOWER, /* 'x' */ C_LOWER, /* 'y' */ C_LOWER, /* 'z' */ C_LOWER,
   /* '{' */ C_PUNCT, /* '|' */ C_PUNCT, /* '}' */ C_PUNCT, /* '~' */ C_PUNCT,
   /* DEL */ C_CTRL,

   /* PAD */ C_CTRL,  /* HOP */ C_CTRL,  /* BPH */ C_CTRL,  /* NBH */ C_CTRL,
   /* IND */ C_CTRL,  /* NEL */ C_CTRL,  /* SSA */ C_CTRL,  /* ESA */ C_CTRL,
   /* HTS */ C_CTRL,  /* HTJ */ C_CTRL,  /* VTS */ C_CTRL,  /* PLD */ C_CTRL,
   /* PLU */ C_CTRL,  /* RI  */ C_CTRL,  /* SS2 */ C_CTRL,  /* SS3 */ C_CTRL,
   /* DCS */ C_CTRL,  /* PU1 */ C_CTRL,  /* PU2 */ C_CTRL,  /* STS */ C_CTRL,
   /* CCH */ C_CTRL,  /* MW  */ C_CTRL,  /* SPA */ C_CTRL,  /* EPA */ C_CTRL,
   /* SOS */ C_CTRL,  /* SCGI*/ C_CTRL,  /* SCI */ C_CTRL,  /* CSI */ C_CTRL,
   /* ST  */ C_CTRL,  /* OSC */ C_CTRL,  /* PM  */ C_CTRL,  /* APC */ C_CTRL,
   /* NBSP*/ C_SPACE,

   /* Punctuation */
   C_PUNCT, C_PUNCT, C_PUNCT, C_PUNCT, C_PUNCT, C_PUNCT, C_PUNCT, C_PUNCT,
   C_PUNCT, C_PUNCT, C_PUNCT, C_PUNCT, C_PUNCT, C_PUNCT, C_PUNCT, C_PUNCT,
   C_PUNCT, C_PUNCT, C_PUNCT, C_PUNCT, C_PUNCT, C_PUNCT, C_PUNCT, C_PUNCT,
   C_PUNCT, C_PUNCT, C_PUNCT, C_PUNCT, C_PUNCT, C_PUNCT, C_PUNCT,

   /* Upper accented */
   C_UPPER, C_UPPER, C_UPPER, C_UPPER, C_UPPER, C_UPPER, C_UPPER, C_UPPER,
   C_UPPER, C_UPPER, C_UPPER, C_UPPER, C_UPPER, C_UPPER, C_UPPER, C_UPPER,
   C_UPPER, C_UPPER, C_UPPER, C_UPPER, C_UPPER, C_UPPER, C_UPPER,

   C_PUNCT,

   /* Upper accented */
   C_UPPER, C_UPPER, C_UPPER, C_UPPER, C_UPPER, C_UPPER, C_UPPER,

   /* Lower accented */
   C_LOWER, C_LOWER, C_LOWER, C_LOWER, C_LOWER, C_LOWER, C_LOWER, C_LOWER,
   C_LOWER, C_LOWER, C_LOWER, C_LOWER, C_LOWER, C_LOWER, C_LOWER, C_LOWER,
   C_LOWER, C_LOWER, C_LOWER, C_LOWER, C_LOWER, C_LOWER, C_LOWER, C_LOWER,

   C_PUNCT,

   /* Lower accented */
   C_LOWER, C_LOWER, C_LOWER, C_LOWER, C_LOWER, C_LOWER, C_LOWER, C_LOWER,
};

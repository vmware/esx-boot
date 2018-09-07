/*******************************************************************************
 * Copyright (c) 2014 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * inet_pton.c --
 *
 *      Checks for a valid IPv6 address according to RFC 5954.
 *      Reference: http://tools.ietf.org/html/rfc5954
 *      The ABNF grammar specified in the RFC is copied here for convenience:
 *      (case #s added by this author for reference):
 *
 * IPv6address =                            6( h16 ":" ) ls32  // Case 0
 *             /                       "::" 5( h16 ":" ) ls32  // Case 1
 *             / [               h16 ] "::" 4( h16 ":" ) ls32  // Case 2
 *             / [ *1( h16 ":" ) h16 ] "::" 3( h16 ":" ) ls32  // Case 3
 *             / [ *2( h16 ":" ) h16 ] "::" 2( h16 ":" ) ls32  // Case 4
 *             / [ *3( h16 ":" ) h16 ] "::"   h16 ":"    ls32  // Case 5
 *             / [ *4( h16 ":" ) h16 ] "::"              ls32  // Case 6
 *             / [ *5( h16 ":" ) h16 ] "::"              h16   // Case 7
 *             / [ *6( h16 ":" ) h16 ] "::"                    // Case 8
 *
 * h16         = 1*4HEXDIG
 * ls32        = ( h16 ":" h16 ) / IPv4address
 * IPv4address = dec-octet "." dec-octet "." dec-octet "." dec-octet
 * dec-octet   = DIGIT              ; 0-9
 *             / %x31-39 DIGIT      ; 10-99
 *             / "1" 2DIGIT         ; 100-199
 *             / "2" %x30-34 DIGIT  ; 200-249
 *             / "25" %x30-35       ; 250-255
 */

#include <sys/types.h>
#include <stdbool.h>
#include <arpa/inet.h>

/*
 *  Internal status codes
 */
#define IPv6_OK            1
#define IPv6_PREFIX        0
#define IPv6_NOMATCH       (-1)

#define INET6_ADDRSTRLEN   46

typedef struct {
   union {
      uint8_t   u8[16];
      uint16_t  u16[8];
      uint32_t  u32[4];
   } addr;
} ipv6_addr;

/*
 *  Token matching MACROs
 *
 *  These all return -1 if no match occurs, otherwise a non-negative value
 */
#define NOMATCH16_t ((int16_t)-1)

#define EOS(s)             (((s) == NULL) || (*(s) == '\0'))
#define MatchCHR(s, match) (!EOS(s) && (*(s) == (match)))
#define MatchDOT(s)        (MatchCHR((s), '.'))
#define MatchCOLON(s)      (MatchCHR((s), ':'))

/*
 * Local MACROs
 */
#define ReturnThisIfNULLorEMPTY(s, this)        \
   do {                                         \
      if (EOS(s)) {                             \
         return (this);                         \
      }                                         \
   } while (0)

#define ReturnFALSEifNULLorEMPTY(s)             \
   ReturnThisIfNULLorEMPTY(s, false)

#define ReturnNOMATCHifNULLorEMPTY(s)           \
   ReturnThisIfNULLorEMPTY(s, IPv6_NOMATCH)

#define DiffPtrs(p1, p2) (((uintptr_t)(p1)) - ((uintptr_t)(p2)))
#define IPv6Result(s)    (*(s) != '\0') ? IPv6_PREFIX : IPv6_OK
#define ResultIPv6FAIL   IPv6_NOMATCH
#define SWAP_U16(u16)    (((u16) << 8) | ((u16) >> 8))

/*-- MatchRANGE ----------------------------------------------------------------
 *
 *      Match a character in the range [lo, hi].
 *
 * Parameters
 *      IN s:       pointer into C-string
 *      IN lo:      low value of range
 *      IN hi:      high value of range
 *      IN adj:     value to add to result
 *      OUT result: result of (*s - lo + adj)
 *
 * Results
 *      'true' on success and 'result' contains (*s - lo + adj).
 *      'false' on failure and 'result' is unchanged.
 * ---------------------------------------------------------------------------*/
static bool MatchRANGE(const char *s, uint8_t lo, uint8_t hi, uint8_t adj,
                       uint8_t *result)
{
   ReturnFALSEifNULLorEMPTY(s);
   if (*s < lo || *s > hi) {
      return false;
   }

   *result = *s - lo + adj;
   return true;
}

/*-- MatchDIGIT ----------------------------------------------------------------
 *
 *      Match a 'DIGIT' token.
 *
 * Parameters
 *      IN s:      pointer into an ASCII string
 *      OUT digit: pointer to result
 *
 * Results
 *      'true' on success and 'digit' contains result 0-9.
 *      'false' on failure and 'hexdig' is unchanged.
 *
 *----------------------------------------------------------------------------*/
static bool MatchDIGIT(const char *s, uint8_t *digit)
{
   return MatchRANGE(s, '0', '9', 0, digit);
}

/*-- MatchHEXDIG ---------------------------------------------------------------
 *
 *      Match a 'HEXDIG' token.
 *
 * Parameters
 *      IN s:       pointer into an ASCII string
 *      OUT hexdig: pointer to result
 *
 * Results
 *      'true' on success and 'hexdig' contains result 0-15.
 *      'false' on failure and 'hexdig' is unchanged.
 *
 *----------------------------------------------------------------------------*/
static bool MatchHEXDIG(const char *s, uint8_t *hexdig)
{
   return (MatchDIGIT(s, hexdig) ||
           MatchRANGE(s, 'A', 'F', 10, hexdig) ||
           MatchRANGE(s, 'a', 'f', 10, hexdig));
}

/*-- MatchDecOctet -------------------------------------------------------------
 *
 *      Match a 'dec-octet' token.
 *
 * Parameters
 *      IN s:       pointer into an ASCII string
 *      OUT octet:  pointer to octet value
 *      OUT nmatch: chars used to match
 *
 * Results
 *      'true' on success and 'octet' contains 0-255 and 'nmatch' contains 1-3.
 *      'false' on failure and 'octet' is unchanged.
 *----------------------------------------------------------------------------*/
static bool MatchDecOctet(const char *s, uint8_t *octet, uint8_t *nmatch)
{
   uint8_t digit, count;
   uint32_t result = 0;

   *nmatch = 0;
   ReturnFALSEifNULLorEMPTY(s);

   // Check up to 3 DIGITs at s without probing past the end of the string...
   for (count = 0; count < 3; ++count) {
      if (!MatchDIGIT(s++, &digit)) {
         break;
      }
      result = result * 10 + digit;      // Shift previous value up and add new
   }

   if (result >= 256) {                         // 256 - 999
      return false;
   } else if (result >= 100) {                  // 100 - 255
      *nmatch = 3;
   } else if (result >= 10 && count == 2) {     //  10 -  99
      *nmatch = 2;
   } else if (count == 1) {                     //   0 -   9
      *nmatch = 1;
   } else {                                     //  000 - 099, 00 - 09
      return false;
   }

   *octet = (uint8_t)result;
   return true;
}

/*-- MatchDecOctetDOT ----------------------------------------------------------
 *
 *      Match a 'dec-octet' token followed by an ASCII dot ('.').
 *
 * Parameters
 *      IN s:       pointer into an ASCII string
 *      OUT octet:  pointer to octet value
 *      OUT nmatch: chars used to match h16
 *
 * Results
 *      'true' on success and 'octet' contains 0-255 and 'nmatch' contains 1-4.
 *      'false' on failure and 'octet' is unchanged.
 *----------------------------------------------------------------------------*/
static bool MatchDecOctetDOT(const char *s, uint8_t *octet, uint8_t *nmatch)
{
   uint8_t n;

   if (MatchDecOctet(s, octet, &n) && MatchDOT(s + n)) {
      *nmatch = n + 1;
      return true;
   }

   return false;
}

/*-- MatchH16 ------------------------------------------------------------------
 *
 *      Match a 'h16' token.
 *
 * Parameters
 *      IN s:       pointer into an ASCII string
 *      OUT h16:    pointer to h16 value
 *      OUT nmatch: chars used to match h16
 *
 * Results
 *      'true' on success and 'h16' contains 0x0-0xffff
 *      and 'nmatch' contains 1-4.
 *      'false' on failure and 'h16' is unchanged.
 *----------------------------------------------------------------------------*/
static bool MatchH16(const char *s, uint16_t *h16, uint8_t *nmatch)
{
   uint8_t hd = 0;                   // hexdigit
   uint8_t count;

   *nmatch = 0;
   ReturnFALSEifNULLorEMPTY(s);

   // Check up to 4 HEXDIGITs at s without probing past the end of the string...
   *h16 = 0;
   for (count = 0; count < 4; ++count) {
      if (!MatchHEXDIG(s++, &hd)) {
         break;
      }
      *h16 = (*h16 << 4) | (hd & 0xf);      // Shift prev. value up and add new
   }

   if (count == 0) {
      return false;
   }

   *h16 = SWAP_U16(*h16);
   *nmatch = count;
   return true;
}

/*-- MatchH16COLON -------------------------------------------------------------
 *
 *      Match a 'h16' token followed by an ASCII colon (':').
 *
 * Parameters
 *      IN s:       pointer into an ASCII string
 *      OUT h16:    pointer to h16 value
 *      OUT nmatch: chars used to match h16
 *
 * Results
 *      'true' on success and 'h16' contains 0x0-0xffff
 *       and 'nmatch' contains 1-4.
 *      'false' on failure and 'h16' is unchanged.
 *----------------------------------------------------------------------------*/
static bool MatchH16COLON(const char *s, uint16_t *h16, uint8_t *nmatch)
{
   uint8_t n;

   if (MatchH16(s, h16, &n) && MatchCOLON(s + n)) {
      *nmatch = n + 1;
      return true;
   }

   return false;
}

/*-- MatchCOLONH16 -------------------------------------------------------------
 *
 *      Match an ASCII colon (':') followed by an 'h16' token.
 *
 * Parameters
 *      IN s:       pointer into an ASCII string
 *      OUT h16:    pointer to h16 value
 *      OUT nmatch: chars used to match h16
 *
 * Results
 *      'true' on success and 'h16' contains 0x0-0xffff
 *       and 'nmatch' contains 1-4.
 *      'false' on failure and 'h16' is unchanged.
 *----------------------------------------------------------------------------*/
static bool MatchCOLONH16(const char *s, uint16_t *h16, uint8_t *nmatch)
{
   uint8_t n;

   if (MatchCOLON(s) && MatchH16(s + 1, h16, &n)) {
      *nmatch = n + 1;
      return true;
   }

   return false;
}

/*-- IPv4address ---------------------------------------------------------------
 *
 *      Match an 'IPv4address' token.
 *
 * Parameters
 *      IN s:       pointer into an ASCII string
 *      OUT ipv4:   pointer to IPv4address value
 *      OUT nmatch: chars used to match
 *
 * Results
 *      'true' on success and 'ipv4' contains 0x0-0xffffffff and
 *      'nmatch' contains 1-15.
 *      'false' on failure and 'ipv4' is unchanged.
 *----------------------------------------------------------------------------*/
static bool MatchIPV4(const char *s, uint32_t *ipv4, uint8_t *nmatch)
{
   uint8_t octet[4], n[4];
   uint8_t i = 0, in = 0;
   const char *s_orig = s;

   *nmatch = 0;
   ReturnFALSEifNULLorEMPTY(s);

   // We need to match 4 parts: [0-255].[0-255].[0-255].[0-255]
   if (!(MatchDecOctetDOT(s,           &octet[i++], &n[in++]) &&
         MatchDecOctetDOT(s + n[0], &octet[i++], &n[in++]) &&
         MatchDecOctetDOT(s + n[0] + n[1], &octet[i++], &n[in++]) &&
         MatchDecOctet   (s + n[0] + n[1] + n[2], &octet[i++], &n[in++])) ) {
      return false;
   }

   s += n[0] + n[1] + n[2] + n[3];
   *ipv4 = octet[3] << 24 | octet[2] << 16 | octet[1] << 8 | octet[0] << 0;
   *nmatch = (uint8_t)DiffPtrs(s, s_orig);
   return true;
}

/*-- MatchIPv6Address ----------------------------------------------------------
 *
 *      Match an 'IPv6address' token.
 *
 * Parameters
 *      IN s:          pointer into an ASCII string
 *      OUT IPv6AddrP: 128-bit IPv6 address
 *
 * Results
 *      IPv6_OK     -- success andthe string was completely consumed
 *                     (all chars matched),
 *      IPv6_PREFIX -- success and the string wasn't completely consumed,
 *      IPv6_NOMATCH on failure.
 *----------------------------------------------------------------------------*/
static int8_t MatchIPv6Address(const char *s, ipv6_addr *IPv6AddrP)
{
   uint8_t n;
   uint32_t ipv4;
   uint8_t preCnt;                // 'h16:' tokens found at 's'
   uint8_t postCnt;               // 'h16:' tokens found after '::'
   uint16_t addrTmp[8];

   ReturnNOMATCHifNULLorEMPTY(s);

   // Initialize the result to all zeros
   for (int i = 0; i < 8; ++i) {
      IPv6AddrP->addr.u16[i] = 0;
      addrTmp[i] = 0;
   }

   /*
    *    --- PREFIX PROCESSING ---
    */
   // Try to match up to seven 'h16 ":"' token sequences at start (could find 0)
   for (preCnt = 0; preCnt < 7; ++preCnt, s += n) {
      if (!MatchH16COLON(s, &IPv6AddrP->addr.u16[preCnt], &n)) {
         n = 0;      // Show we matched zero chars this round...
         break;
      }
   }

   if (preCnt == 7) {         // T -> possible cases are Case 8 or Case 0
      // Check Case 0
      if (MatchH16(s, &IPv6AddrP->addr.u16[7], &n)) {
         return IPv6Result(s + n);

      // Check Case 8
      } else if (MatchCOLON(s)) {
         return IPv6Result(s + 1);
      }

      return (*s == 0) ? IPv6_NOMATCH : IPv6_PREFIX;

   } else if (preCnt == 6) {       // T -> cases are Case 0 with IPv4, or Case 7
      // Check Case 0
      if (MatchIPV4(s, &IPv6AddrP->addr.u32[3], &n)) {
         return IPv6Result(s + n);

      // Check Case 7
      } else if (MatchCOLONH16(s, &IPv6AddrP->addr.u16[7], &n)) {
         return IPv6Result(s + n);

      // Check Case 8
      } else if (MatchCOLON(s)) {
         return IPv6Result(s + 1);
      }

      return (*s == 0) ? IPv6_NOMATCH : IPv6_PREFIX;
   }

   /*
    *    --- POSTFIX PROCESSING ---
    */
   // If no tokens matched yet, next token must be a colon...
   if (preCnt == 0) {
      if (!MatchCOLON(s)) {
         return IPv6_NOMATCH;
      }
      ++s;
   }

   // Try to match up to 7-preCnt '":" h16' tokens.
   for (postCnt = 0; postCnt < (7 - preCnt); ++postCnt, s += n) {
      if (!MatchCOLONH16(s, &addrTmp[postCnt], &n)) {
         break;
      }
   }

   // If no tokens matched yet, next token must be a colon.  Case 8.
   if (postCnt == 0) {
      if (!MatchCOLON(s)) {
         return IPv6_NOMATCH;
      }

      return IPv6Result(s + 1);
   }

   // Check if the last : H16 matched part of an IPv4, back up
   if (!EOS(s) && (*s == '.')) {
      s = s - n + 1; // Had 'h16 ":" dec-octet "."' and n covers '":" dec-octet'
      --postCnt;     // Back up one postfix token

      if (!MatchIPV4(s, &ipv4, &n)) {  // T -> s doesn't point to a IPv4 address
         return IPv6_NOMATCH;
      }

      s += n;
      addrTmp[postCnt++] = (uint16_t)ipv4;
      addrTmp[postCnt++] = (uint16_t)(ipv4 >> 16);
   }

   // Write the last thing in 'addrTmp' to the end
   // of IPv6AddrP->addr.u16 and work backwards...
   for (int i = postCnt - 1, j = 7; i >= 0; --i, --j) {
      IPv6AddrP->addr.u16[j] = addrTmp[i];
   }

   return IPv6Result(s);                                              // Case 8
}

/*-- MatchIPv4Address ----------------------------------------------------------
 *
 *      Determines if 's' points to a valid IPv4 address.
 *
 * Parameters
 *      IN s:     pointer into an ASCII string
 *      OUT ipv4: 32-bit IPv4 address
 *
 * Results
 *      IPv6_OK if s is a valid IPv6 address and was completely used.
 *      IPv6_PREFIX if s is a valid IPv6 address and was NOT completely used.
 *      IPv6_NOMATCH if s isn't a valid IPv6.
 *----------------------------------------------------------------------------*/
static int8_t MatchIPv4Address(const char *s, uint32_t *ipv4)
{
   uint8_t n;

   ReturnNOMATCHifNULLorEMPTY(s);
   if (MatchIPV4(s, ipv4, &n)) {
      return IPv6Result(s + n);
   }

   return IPv6_PREFIX;
}

/*-- inet_pton -----------------------------------------------------------------
 *
 *     Converts an ASCII string into a network address structure in the AF
 *     address family.
 *
 * Parameters
 *      IN af:   AF family (must be either AF_INET or AF_INET6)
 *      IN src:  pointer to an ASCII string
 *      OUT dst: pointer to a 128-bit buffer
 *
 * Results
 *      1 on success (network address was successfully converted), or 0
 *      otherwise.
 *----------------------------------------------------------------------------*/
int inet_pton(int af, const char *src, void *dst)
{
   int status;

   if (af == AF_INET6) {
      status = MatchIPv6Address(src, dst);
   } else if (af == AF_INET) {
      status = MatchIPv4Address(src, dst);
   } else {
      return -1;
   }

   return (status == IPv6_OK || status == IPv6_PREFIX) ? 1 : 0;
}

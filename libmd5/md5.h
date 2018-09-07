/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 */

/*
 * Unable to find the original Colin Plumb implementation, I had
 * to settle for a copy that a certain John Walker had mangled
 * from ANSI C into K&R C and then unmangle it. --plangdale
 *
 * md5 is no longer considered a safe hashing algorithm for
 * cryptographic purposes, and should not be used in such
 * cases. We have SHA1 and SHA256 as better choices. But it
 * does have non-cryptographic uses, hence it's presence here.
 *
 */

#ifndef _MD5_H_
#define _MD5_H_

#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <compat.h>

#define MD5_HASH_LEN     16
#define MD5_STRING_LEN   (2 * MD5_HASH_LEN + 1)

typedef struct {
        uint32_t buf[4];
        uint32_t bits[2];
        unsigned char in[64];
} MD5_CTX;

typedef unsigned char md5_t[MD5_HASH_LEN];

void MD5Init(MD5_CTX *ctx);
void MD5Update(MD5_CTX *ctx, const unsigned char *data, unsigned int len);
void MD5Final(unsigned char digest[MD5_HASH_LEN], MD5_CTX *ctx);

/*-- md5_to_str ---------------------------------------------------------------
 *
 *      Convert the raw md5 sum into a readable string.
 *
 * Parameters
 *      IN md5_raw:  Raw md5 sum.
 *      OUT md5_str: Formatted md5 in a readable string.
 *      IN size:     Size of the output buffer.
 *
 * Results
 *      Pointer to the buffer containing the md5 string.
 *----------------------------------------------------------------------------*/
INLINE char *md5_to_str(const md5_t *md5_raw, char *md5_str, size_t size)
{
   unsigned int i;
   const unsigned char *md5 = (const unsigned char *)md5_raw;

   if (md5_str == NULL) {
      return NULL;
   }

   for (i = 0; i < MD5_HASH_LEN && (2 * i + 3) <= size; i++) {
      snprintf(md5_str + (2 * i), 3, "%02x", md5[i]);
   }

   return md5_str;
}

/*-- compute_md5 ---------------------------------------------------------------
 *
 *      Compute the md5 of given data.
 *
 * Parameters
 *      IN data:    Buffer containing the data over which md5 sum is needed.
 *      IN len:     Size of data in above buffer in bytes.
 *      OUT md5sum: Buffer containing the md5 sum.
 *----------------------------------------------------------------------------*/
INLINE void md5_compute(void *data, size_t len, md5_t *md5sum)
{
   MD5_CTX ctx;
   unsigned char *p = data;
   unsigned int chunk_size;

   if (md5sum == NULL || (data == NULL && len != 0)) {
      return;
   }

   MD5Init(&ctx);

   do {
      chunk_size = MIN(len, UINT_MAX);
      MD5Update(&ctx, p, chunk_size);
      len -= chunk_size;
      p += chunk_size;
   } while(len > 0);

   MD5Final(*md5sum, &ctx);
}

#endif /* _MD5_H_ */

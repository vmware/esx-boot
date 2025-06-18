/*******************************************************************************
 * Copyright (c) 2008-2024 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * gzip.c -- Gzip extraction support
 */

#include <stdbool.h>
#include <zlib.h>
#include <bootlib.h>
#include <boot_services.h>

#define GZIP_BYTE_0            0x1f
#define GZIP_BYTE_1            0x8b

#define GZIP_FLAG_ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define GZIP_FLAG_HEADER_CRC   0x02 /* bit 1 set: header CRC present */
#define GZIP_FLAG_EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define GZIP_FLAG_ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define GZIP_FLAG_COMMENT      0x10 /* bit 4 set: file comment present */
#define GZIP_FLAG_RESERVED     0xE0 /* bits 5..7: reserved */

/*-- error_zlib_to_generic -----------------------------------------------------
 *
 *      Convert a Zlib error number to a generic status code.
 *
 * Parameter
 *      IN err: the Zlib error value
 *
 * Results
 *      The generic status code.
 *----------------------------------------------------------------------------*/
static int error_zlib_to_generic(int err)
{
   switch (err) {
      case Z_OK:
         return ERR_SUCCESS;
      case Z_ERRNO:
         return ERR_UNKNOWN;
      case Z_VERSION_ERROR:
         return ERR_INCOMPATIBLE_VERSION;
      case Z_DATA_ERROR:
         return ERR_INCONSISTENT_DATA;
      case Z_MEM_ERROR:
         return ERR_OUT_OF_RESOURCES;
      case Z_BUF_ERROR:
         return ERR_BUFFER_TOO_SMALL;
      case Z_STREAM_ERROR:
      default:
         return ERR_INVALID_PARAMETER;
   }
}

/*-- gzip_header_size ----------------------------------------------------------
 *
 *      Get the size of a gzip header.
 *
 * Parameters
 *      IN  buffer:   pointer to the compressed data
 *      IN  filesize: size of the compressed data
 *      OUT hdr_size: the header size in bytes
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int gzip_header_size(const void *buffer, size_t filesize,
                            size_t *hdr_size)
{
   int flags;
   uInt len;
   const uint8_t *hdr = buffer;

   *hdr_size = 0;

   if (filesize < 2 || hdr[0] != GZIP_BYTE_0 || hdr[1] != GZIP_BYTE_1) {
      return ERR_BAD_TYPE;
   }

   if ((filesize < 10) || ((hdr[3] & GZIP_FLAG_RESERVED) != 0)) {
      return ERR_BAD_HEADER;
   }

   if (hdr[2] != Z_DEFLATED) {
       return ERR_UNSUPPORTED;
   }

   flags = (int)hdr[3];
   len = 10;

   if ((flags & GZIP_FLAG_EXTRA_FIELD) != 0) {
      if (filesize < 13) {
         return ERR_BAD_HEADER;
      }
      len += 2 + (uInt)hdr[10] + ((uInt)hdr[11] << 8);
      if (filesize < len) {
         return ERR_BAD_HEADER;
      }
   }

   if ((flags & GZIP_FLAG_ORIG_NAME) != 0) {
      do {
         if (++len > filesize) {
            return ERR_BAD_HEADER;
         }
      } while (hdr[len - 1] != '\0');
   }

   if ((flags & GZIP_FLAG_COMMENT) != 0) {
      do {
         if (++len > filesize) {
            return ERR_BAD_HEADER;
         }
      } while (hdr[len - 1] != '\0');
   }

   if ((flags & GZIP_FLAG_HEADER_CRC) != 0) {
      if (len + 2 > filesize) {
         return ERR_BAD_HEADER;
      }
      len += 2;
   }

   *hdr_size = (size_t)len;

   return ERR_SUCCESS;
}

/*-- gzip_get_info -------------------------------------------------------------
 *
 *      Get the original size of the gzip archive & CRC from input buffer. The
 *      original size is the size that is needed in order to extract the gzip
 *      archive. According to the RFC 1952, this size is located into the last 4
 *      bytes of the archive. CRC is 4 bytes and is located prior to size.
 *
 * Parameters
 *      IN  buffer:   pointer to the archive
 *      IN  filesize: size of the compressed archive
 *      IN  hdrsize:  the header size in bytes
 *      OUT size:     The original size (in bytes).
 *      OUT csum:     Checksum of the compressed inout data.
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int gzip_get_info(const void *buffer, size_t filesize, size_t hdrsize,
                         size_t *size, uint32_t *csum)
{
   const char *p;
   uint32_t osize;

   /*
    * Validate that the input filesize is large enough to hold gzip header,
    * gzipped input buffer(0 or more bytes), CRC and extracted filesize.
    * CRC and output filesize are at the end of gzipped buffer. filesize is
    * the last 4 bytes and CRC is the 4 bytes prior to filesize.
    */
   if ((filesize - (hdrsize + 8)) <= 0) {
      return ERR_INCONSISTENT_DATA;
   }

   p = (const char *)buffer + filesize - 8; /* CRC checksum */
   memcpy(&osize, p, sizeof osize);
   *csum = osize;
   p = (const char *)buffer + filesize - 4; /* filesize */
   memcpy(&osize,  p, sizeof osize);
   *size = (size_t)osize;

   return ERR_SUCCESS;
}

/*-- gunzip_buffer -------------------------------------------------------------
 *
 *      Buffer to buffer extraction.
 *
 * Parameters
 *      IN  source:    pointer to the compressed data
 *      IN  sourceLen: size of the compressed data
 *      IN  dest:      destination buffer
 *      IN  destLen:   size of the destination buffer
 *      OUT destLen:   number of bytes that have been written into the
 *                     destination buffer
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int gunzip_buffer(const void *source, size_t sourceLen, void *dest,
                         size_t *destLen)
{
   z_stream stream;
   int err;

   stream.next_in = (Bytef *)source;
   stream.avail_in = (uInt)sourceLen;
   stream.next_out = dest;
   stream.avail_out = (uInt)*destLen;
   stream.zalloc = Z_NULL;
   stream.zfree = Z_NULL;
   stream.opaque = Z_NULL;

   err = inflateInit2(&(stream), -MAX_WBITS);
   if (err != Z_OK) {
      return error_zlib_to_generic(err);
   }

   err = inflate(&stream, Z_FINISH);
   inflateEnd(&stream);
   if (err != Z_STREAM_END) {
      return error_zlib_to_generic(err);
   }

   *destLen = (size_t)stream.total_out;

   return ERR_SUCCESS;
}

/*-- gzip_extract --------------------------------------------------------------
 *
 *      Buffer to buffer gzip extraction. The output buffer is dynamically
 *      allocated, or points to the input buffer if the input data are not a
 *      gzip archive.
 *
 * Parameters
 *      IN  ibuffer: pointer to the gzip'ed data
 *      IN  isize:   size of the gzip'ed data
 *      OUT obuffer: pointer to the freshly allocated extracted data
 *      OUT osize:   size of the extracted data
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int gzip_extract(const void *ibuffer, size_t isize,
                 void **obuffer, size_t *osize)
{
   void *output;
   size_t header_len;
   size_t size = 0;
   int status;
   uint32_t received_crc, calculated_crc;

   status = gzip_header_size(ibuffer, isize, &header_len);
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "Error %d (%s) while parsing gzip header\n",
          status, error_str[status]);
      return status;
   }

   status = gzip_get_info(ibuffer, isize, header_len, &size, &received_crc);
   if (status != ERR_SUCCESS) {
      Log(LOG_ERR, "Error %d (%s) reading original filesize or received crc\n",
          status, error_str[status]);
      return status;
   }
   if (size == 0) {
      size_t actual_isize = isize - (header_len + 8);
      if (actual_isize > 256) {
         Log(LOG_ERR, "Module content is likely corrupt.\n");
         Log(LOG_ERR, "isize: %zu, hdrlen: %zu, recdCRC: %u, osize: %zu ",
             isize, header_len, received_crc, size);
         return ERR_CRC_ERROR;
      }
      *obuffer = NULL;
      *osize = 0;
      return ERR_SUCCESS;
   }

   /* ibuffer starts after the gzip header */
   ibuffer = (char *)ibuffer + header_len;
   /*
    * ibuffer ends before the CRC and output size (4 + 4 bytes). gzip requires
    * an extra "dummy" byte at the end of the input buffer, after the end of
    * the compressed data. See the comments in the zlib source, near
    * inflateInit2 in gzio.c.
    */
   isize -= (header_len + 8 - 1);

   output = malloc(size);

   if (output == NULL) {
      Log(LOG_ERR, "Out of resources for decompressing data(%zu)\n", size);
      return ERR_OUT_OF_RESOURCES;
   }

   status = gunzip_buffer(ibuffer, isize, output, &size);
   if (status != ERR_SUCCESS) {
      free(output);
      Log(LOG_ERR, "Error %d (%s) while decompressing data\n",
          status, error_str[status]);
      Log(LOG_ERR, "  input(%zu), output(%zu)\n", isize, size);
      return status;
   }

   calculated_crc = crc32(0, output, size);

   if (received_crc != calculated_crc) {
      *obuffer = NULL;
      *osize = 0;
      free(output);
      Log(LOG_ERR, "CRC error during decompression. Received CRC (0x%x) != "
                   "calculated CRC (0x%x)\n",received_crc, calculated_crc);
      return ERR_CRC_ERROR;
   }

   *obuffer = output;
   *osize = size;
#ifdef DEBUG
   Log(LOG_DEBUG, "recdCRC 0x%x, calcCRC 0x%x, tSize %zu, eSize %zu\n",
       received_crc, calculated_crc, isize, size);
#endif
   return ERR_SUCCESS;
}

/*-- is_gzip -------------------------------------------------------------------
 *
 *      Check whether the given buffer contains a gzip archive.
 *
 * Parameters
 *       IN  buffer:   data buffer
 *       IN  filesize: data size
 *       OUT status:   header status
 *
 * Results
 *       true if buffer is a gzip archive, false otherwise.
 *----------------------------------------------------------------------------*/
bool is_gzip(const void *buffer, size_t bufsize, int *status)
{
   size_t hdr_size;

   *status = gzip_header_size(buffer, bufsize, &hdr_size);
   return *status == ERR_SUCCESS ? true : false;
}

/*******************************************************************************
 * Copyright (c) 2008-2011 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * file.c -- File access support
 */

#include <string.h>
#include <limits.h>
#include "com32_private.h"

#define COM32_READ_BLOCK_SIZE_MAX   (16 * 1024)
#define COM32_READ_BLOCK_COUNT_MAX  UINT16_MAX
#define REALLOC_CHUNK_SIZE          (1024 * 1024)

#define BYTES_TO_BLOCKS(_bytes_, _blksize_) \
   (((_bytes_) + (_blksize_) - 1) / (_blksize_))

#define IS_POWER_OF_TWO(x)          (((x) & ((x) - 1)) == 0)

/*-- com32_is_valid_file_block_size --------------------------------------------
 *
 *      COM32 block size sanity checks. The Syslinux file system is block-
 *      oriented. The size of a block will always be a power of two and no
 *      greater than 16K.
 *
 * Parameters
 *      IN blk_size: file block size, in bytes
 *
 * Results
 *      True is the block size is valid, false otherwise.
 *----------------------------------------------------------------------------*/
static INLINE bool com32_is_valid_file_block_size(size_t blk_size)
{
   return (blk_size > 0 && IS_POWER_OF_TWO(blk_size) &&
           blk_size <= COM32_READ_BLOCK_SIZE_MAX);
}

/*-- com32_read_max_blocks -----------------------------------------------------
 *
 *      Return the maximum number of blocks that can be read in only one read
 *      operation. The size of a read operation is limited by both the theorical
 *      maximum block count, and the bounce buffer size.
 *
 * Parameters
 *      IN blk_size: file block size, in bytes
 *
 * Results
 *      The maximum number of blocks.
 *----------------------------------------------------------------------------*/
static size_t com32_read_max_blocks(size_t blk_size)
{
   return MIN(get_bounce_buffer_size() / blk_size, COM32_READ_BLOCK_COUNT_MAX);
}

/*-- com32_fopen ---------------------------------------------------------------
 *
 *      Wrapper for the 'Open file' COM32 service.
 *
 *      NOTE: Syslinux considers a zero-length file to be nonexistent. Hence, an
 *            actual empty file makes this function return ERR_NOT_FOUND. On the
 *            other hand, if a non-empty file is found but its size cannot be
 *            determined, this function returns ERR_SUCCESS, and filesize
 *            contains zero.
 *
 * Parameters
 *      IN  filepath: path to the file
 *      OUT fd:       file handle to be passed to com32_fread()/com32_fclose()
 *      OUT filesize: file size in bytes or 0 if the size cannot be determined
 *      OUT blk_size: read block size, in bytes
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int com32_fopen(const char *filepath, uint16_t *fd, size_t *filesize,
                       size_t *blk_size)
{
   com32sys_t iregs, oregs;
   uint16_t handle, block;
   uint32_t size;
   farptr_t fptr;
   int status;
   char *name;

   if (strlen(filepath) >= get_bounce_buffer_size()) {
      return ERR_INVALID_PARAMETER;
   }

   name = get_bounce_buffer();
   strcpy(name, filepath);

   memset(&iregs, 0, sizeof (iregs));
   fptr = virtual_to_real(name);
   iregs.eax.w[0] = 0x06;
   iregs.es = fptr.real.segment;
   iregs.esi.w[0] = fptr.real.offset;
   status = intcall_check_CF(COM32_INT, &iregs, &oregs);
   if (status != ERR_SUCCESS) {
      return ERR_NOT_FOUND;
   }

   handle = oregs.esi.w[0];
   if (handle == 0) {
      return ERR_NOT_FOUND;
   }

   size = oregs.eax.l;

   if (size == 0) {
      return ERR_NOT_FOUND;
   } else if (size == (uint32_t)(-1)) {
      /*
       * In 3.70 or later, EAX can contain -1 indicating that the file length is
       * unknown.
       */
      size = 0;
   }

   block = oregs.ecx.w[0];
   if (!com32_is_valid_file_block_size(block)) {
      return ERR_DEVICE_ERROR;
   }

   *filesize = (size_t)size;
   *fd = handle;
   *blk_size = (size_t)block;

   return ERR_SUCCESS;
}

/*-- com32_fclose --------------------------------------------------------------
 *
 *      Wrapper for the 'Close file' COM32 service.
 *
 *      NOTE: If end of file was reached, the file was automatically closed.
 *
 * Parameters
 *      IN fd: file handle returned by com32_fopen()
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int com32_fclose(uint16_t fd)
{
   com32sys_t iregs;

   if (fd == 0) {
      return ERR_SUCCESS;
   }

   memset(&iregs, 0, sizeof (iregs));
   iregs.eax.w[0] = 0x08;
   iregs.esi.w[0] = fd;

   return intcall_check_CF(COM32_INT, &iregs, NULL);
}

/*-- com32_fread ---------------------------------------------------------------
 *
 *      Wrapper for the 'Read file' COM32 service. The provided output buffer
 *      must be large enough to accomodate either (blk_size * count) bytes or
 *      *buflen bytes, whichever is smaller.
 *
 *      NOTE: If end of file was reached, the file was automatically closed.
 *
 * Parameters
 *      IN  fd:       pointer to the file handle returned by com32_fopen()
 *      IN  blk_size: file block size as returned by com32_fopen()
 *      IN  count:    number of blocks to read
 *      IN  buffer:   pointer to the output buffer
 *      IN  buflen:   output buffer size, in bytes
 *      OUT fd:       Updated file handle
 *      OUT buflen:   number of bytes actually written into buffer
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int com32_fread(uint16_t *fd, size_t blk_size, size_t count,
                       bool is_gpxe, void *buffer, size_t *buflen)
{
   com32sys_t iregs, oregs;
   void *bounce_buf;
   farptr_t fptr;
   size_t size;
   int status;

   if (count == 0) {
      *buflen = 0;
      return ERR_SUCCESS;
   }

   if (*fd == 0 || !com32_is_valid_file_block_size(blk_size) ||
       count > com32_read_max_blocks(blk_size)) {
      return ERR_INVALID_PARAMETER;
   }

   bounce_buf = get_bounce_buffer();

   memset(&iregs, 0, sizeof (iregs));
   fptr = virtual_to_real(bounce_buf);
   iregs.eax.w[0] = 0x07;
   iregs.es = fptr.real.segment;
   iregs.ebx.w[0] = fptr.real.offset;
   iregs.esi.w[0] = *fd;
   iregs.ecx.w[0] = count;
   status = intcall_check_CF(COM32_INT, &iregs, &oregs);
   if (status != ERR_SUCCESS) {
      return status;
   }

   *fd = oregs.esi.w[0];

   if (is_gpxe || com32.major > 3 || (com32.major == 3 && com32.minor >= 70)) {
      /*
       * In 3.70 or later, ECX returns the number of bytes read. This will
       * always be a multiple of the block size unless with gPXE, or if EOF is
       * reached.
       */
      size = (size_t)oregs.ecx.l;
      if ((size > get_bounce_buffer_size()) ||
          (!is_gpxe && *fd != 0 && ((size % blk_size) != 0))) {
         return ERR_DEVICE_ERROR;
      }

      if (size > *buflen) {
         return ERR_BUFFER_TOO_SMALL;
      }
   } else {
      /*
       * Versions before 3.70, do not return the number of bytes read in ECX.
       * If the intcall has succeeded, we can assume than all blocks have been
       * read, unless EOF was reached.
       */
      size = MIN(count * blk_size, *buflen);
      if (size > get_bounce_buffer_size()) {
         return ERR_INVALID_PARAMETER;
      }
   }

   memcpy(buffer, bounce_buf, size);
   *buflen = size;

   return ERR_SUCCESS;
}

/*-- firmware_file_get_size_hint -----------------------------------------------
 *
 *      Try to get the size of a file.
 *
 *      gPXE note:
 *         File sizes cannot be determined in advance with gPXE because the
 *         HTTP protocol allows file sizes to vary (e.g. CGI scripts). Then,
 *         using fstat() makes gPXE download the whole file in order to
 *         determine its size.
 *
 * Parameters
 *      IN  filepath: absolute path of the file
 *      OUT filesize: the file size, in bytes
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int firmware_file_get_size_hint(const char *filepath, size_t *filesize)
{
   size_t blk_size, size;
   uint16_t fd;
   int status;

   if (isGPXE()) {
      return ERR_UNSUPPORTED;
   }

   status = com32_fopen(filepath, &fd, &size, &blk_size);
   if (status != ERR_SUCCESS) {
      return status;
   }

   com32_fclose(fd);

   if (size == 0) {
      return ERR_UNSUPPORTED;
   }

   *filesize = size;

   return ERR_SUCCESS;
}

/*-- file_load_unbounded -------------------------------------------------------
 *
 *      Read a file whose size is unknown. This is useful for loading files via
 *      gPXE (which downloads the whole file for determining its size), or via
 *      PXE with old servers which do not support the 'tsize' PXE option.
 *
 *      Initialization
 *        1. max_blocks = Maximum blocks that can be read in a single operation
 *        2. n = 0
 *
 *      Main loop
 *        3. Do we have enough space for reading max_blocks?
 *           - yes -> jumps to step (4)
 *           - no  -> allocate ++n more MB
 *        4. Reads max_blocks
 *        5. EOF?
 *            - Yes -> We're done loading.
 *            - No  -> jump back to step (3)
 *
 *      Growing the buffer by (n * 1MB) limits the number of realloc()
 *      iterations, and also avoids wasting too much memory as if we were
 *      doubling the buffer size at every iteration.
 *
 * Parameters
 *      IN  fd:       file descriptor
 *      IN  blk_size: file block size, in bytes
 *      IN  is_gpxe:  are we gPXE-booted?
 *      OUT filesize: number of bytes actually written into buffer
 *      OUT buffer:   pointer to the freshly allocated output buffer
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int file_load_unbounded(uint16_t *fd, size_t blk_size, bool is_gpxe,
                               size_t *filesize, void **buffer)
{
   size_t blocks, buf_size, grow_size, len, offset, max_blocks;
   char *buf, *tmp;
   int status;

   buf = NULL;
   grow_size = 0;
   offset = 0;
   buf_size = 0;
   max_blocks = com32_read_max_blocks(blk_size);

   while (*fd != 0) {
      if (buf_size - offset < max_blocks * blk_size) {
         grow_size += REALLOC_CHUNK_SIZE;
         tmp = sys_realloc(buf, buf_size, buf_size + grow_size);
         if (tmp == NULL) {
            status = ERR_OUT_OF_RESOURCES;
            goto error;
         }
         buf = tmp;
         buf_size += grow_size;
      }

      blocks = MIN(max_blocks, (buf_size - offset) / blk_size);
      len = blocks * blk_size;

      status = com32_fread(fd, blk_size, blocks, is_gpxe, buf + offset, &len);
      if (status != ERR_SUCCESS) {
         goto error;
      }

      if (is_gpxe && len == 0 && *fd != 0) {
         /*
          * gPXE returns 0 in ECX when reaching EOF. in addition, gPXE doees not
          * automatically close the file descriptor at EOF.
          */
         com32_fclose(*fd);
         *fd = 0;
      }

      offset += len;
   }

   tmp = sys_realloc(buf, buf_size, offset);
   if (tmp == NULL) {
      status = ERR_OUT_OF_RESOURCES;
      goto error;
   }

   *buffer = tmp;
   *filesize = offset;

   return ERR_SUCCESS;

 error:
   sys_free(buf);
   return status;
}

/*-- file_load_bounded ---------------------------------------------------------
 *
 *      Read a file whose size is known.
 *
 * Parameters
 *      IN  fd:       file handle as returned by com32_fopen()
 *      IN  blk_size: file block size, in bytes
 *      IN  is_gpxe:  are we booted via gPXE?
 *      IN  callback: routine to be called periodically while the file is being
 *                    loaded
 *      IN  filesize: the expected file size, in bytes
 *      OUT buffer:   pointer to the freshly allocated output buffer
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int file_load_bounded(uint16_t *fd, size_t blk_size, bool is_gpxe,
                             int (*callback)(size_t), size_t filesize,
                             void **buffer)
{
   size_t blocks, len, offset, max_blocks, start;
   int status;
   char *buf;

   buf = sys_malloc(BYTES_TO_BLOCKS(filesize, blk_size) * blk_size);
   if (buf == NULL) {
      return ERR_OUT_OF_RESOURCES;
   }

   status = ERR_SUCCESS;
   offset = 0;
   start = 0;
   max_blocks = com32_read_max_blocks(blk_size);

   while (*fd != 0) {
      blocks = MIN(BYTES_TO_BLOCKS(filesize - offset, blk_size), max_blocks);
      len = MIN(blocks * blk_size, filesize - offset);

      status = com32_fread(fd, blk_size, blocks, is_gpxe, buf + offset, &len);
      if (status != ERR_SUCCESS) {
         break;
      }

      if (is_gpxe && len == 0 && *fd != 0) {
         /*
          * gPXE returns 0 in ECX when reaching EOF. in addition, gPXE does not
          * automatically close the file descriptor at EOF.
          */
         com32_fclose(*fd);
         *fd = 0;
      }

      offset += len;
      if (offset > filesize) {
         status = ERR_LOAD_ERROR;
         break;
      }

      if (*fd == 0 || offset - start >= READ_CHUNK_SIZE) {
         if (callback != NULL) {
            status = callback(offset - start);
            if (status != ERR_SUCCESS) {
               break;
            }
         }
         start = offset;
      }
   }

   if (status == ERR_SUCCESS) {
      if (offset < filesize) {
         status = ERR_UNEXPECTED_EOF;
      } else if (*fd != 0) {
         status = ERR_LOAD_ERROR;
      }
   }

   if (status != ERR_SUCCESS) {
      sys_free(buf);
      return status;
   }

   *buffer = buf;

   return ERR_SUCCESS;
}

/*-- firmware_file_read --------------------------------------------------------
 *
 *      Read an entire file.
 *
 * Parameters
 *      IN  filepath: absolute path of the file
 *      IN  callback: routine to be called periodically while the file is being
 *                    loaded
 *      OUT buffer:   pointer to the freshly allocated output buffer
 *      OUT bufsize:  number of bytes actually written into buffer
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int firmware_file_read(const char *filepath, int (*callback)(size_t),
                       void **buffer, size_t *bufsize)
{
   size_t blk_size, size;
   bool is_gpxe;
   uint16_t fd;
   int status;

   status = com32_fopen(filepath, &fd, &size, &blk_size);
   if (status != ERR_SUCCESS) {
      return status;
   }

   is_gpxe = (com32.derivative == COM32_DERIVATIVE_GPXE);

   if (size > 0) {
      status = file_load_bounded(&fd, blk_size, is_gpxe, callback, size,
                                 buffer);
   } else {
      status = file_load_unbounded(&fd, blk_size, is_gpxe, &size, buffer);
   }

   if (status != ERR_SUCCESS || fd != 0) {
      com32_fclose(fd);
   }

   if (status == ERR_SUCCESS) {
      *bufsize = size;
   }

   return status;
}

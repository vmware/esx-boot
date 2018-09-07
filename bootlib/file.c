/*******************************************************************************
 * Copyright (c) 2008-2013,2016 VMware, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 ******************************************************************************/

/*
 * file.c -- File access
 */

#include <ctype.h>
#include <boot_services.h>
#include <bootlib.h>
#include <libfat.h>
#include <libfatint.h>
#include <md5.h>

#define FAT_SHORT_NAME_LEN      11

typedef struct {
   disk_t *disk;
   partition_t *partition;
} volume_t;

static disk_t disk_info;
static partition_t part_info;
static volume_t dfd;

/*-- partition_read_handler ----------------------------------------------------
 *
 *      Handler used by the libfat read() function to read disk sectors on a FAT
 *      partition.
 *
 * Parameters
 *      IN readptr: file system descriptor address
 *      IN buffer:  pointer to the output buffer
 *      IN size:    number of bytes to read
 *      IN sector:  first sector to read from (relative to the partition)
 *
 * Results
 *      The number of read bytes, or -1 if an error occurred.
 *----------------------------------------------------------------------------*/
static int partition_read_handler(intptr_t readptr, void *buffer, size_t size,
                                  libfat_sector_t sector)
{
   volume_t *vol;
   size_t count;
   int status;

   vol = (volume_t *)readptr;

   sector += vol->partition->info.start_lba;
   count = ceil(size, vol->disk->bytes_per_sector);

   status = disk_read(vol->disk, buffer, sector, count);
   if (status != ERR_SUCCESS) {
      return -1;
   }

   return (int)size;
}

/*-- fat_fread_sectors ---------------------------------------------------------
 *
 *      Read file sectors form a FAT file system.
 *
 * Parameters
 *      IN  fs:     pointer to the FAT filesystem info structure
 *      IN  buffer: pointer to the output buffer
 *      IN  sector: first sector to read from
 *      IN  count:  number of sectors to read
 *      OUT sector: next sector to read
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int fat_fread_sectors(struct libfat_filesystem *fs, void *buffer,
                             libfat_sector_t *sector, size_t count)
{
   libfat_sector_t next, start;
   size_t len, n, numsectors;
   char *bufp;
   uint32_t bytes_per_sector;

   start = *sector;
   bufp = buffer;
   bytes_per_sector = fs->bytes_per_sector;

   for (n = 0; n < count; n += numsectors) {
      for (numsectors = 1; numsectors < count - n; numsectors++) {
         next = libfat_nextsector(fs, start + numsectors - 1);
         if (next == (libfat_sector_t)(-1)) {
            return ERR_VOLUME_CORRUPTED;
         } else if (next != start + numsectors) {
            break;
         }
      }

      len = numsectors * bytes_per_sector;
      if (fs->read(fs->readptr, bufp, len, start) != (int)len) {
         return ERR_DEVICE_ERROR;
      }

      start = libfat_nextsector(fs, start + numsectors - 1);
      if (start == (libfat_sector_t)(-1)) {
         return ERR_UNEXPECTED_EOF;
      }

      bufp += len;
   }

   *sector = start;

   return ERR_SUCCESS;
}

/*-- fat_get_shortname ---------------------------------------------------------
 *
 *      Convert a filename to an 11-bytes FAT short name.
 *
 * Parameters
 *      IN name:      original filename
 *      IN shortname: pointer to an 11-bytes buffer
 *----------------------------------------------------------------------------*/
void fat_get_shortname(const char *name, char *shortname)
{
   char *end_destptr, *destptr;
   const char *srcptr;

   while (*name == '/') {
      name++;
   }

   if (strlen(name) > FAT_SHORT_NAME_LEN) {
      shortname[0] = '\0';
   }

   srcptr = name;
   destptr = shortname;
   end_destptr = destptr + 8;

   while (*srcptr != '\0' && *srcptr != '.' && destptr < end_destptr) {
      if (*srcptr != ' ') {
         *(destptr++) = toupper(*srcptr);
      }
      srcptr++;
   }

   /* Pad the name with spaces. */
   while (destptr < end_destptr) {
      *(destptr++) = ' ';
   }

   /* Copy the file suffix. */
   end_destptr = destptr + 3;
   while (*srcptr != '\0' && *srcptr != '.') {
      srcptr++;
   }

   if (*srcptr == '.') {
      while ((*(++srcptr)) && (destptr < end_destptr)) {
         if (*srcptr != ' ') {
            *(destptr++) = toupper(*srcptr);
         }
      }
   }

   /* Pad the suffix with spaces */
   while (destptr < end_destptr) {
      *(destptr++) = ' ';
   }
}

/*-- fat_file_open -------------------------------------------------------------
 *
 *      Open a file on a FAT filesystem.
 *
 * Parameters
 *      IN  volid:    MBR/GPT partition number of the volume to load from
 *      IN  filename: absolute path to the file
 *      OUT fsinfo:   newly created FAT filesystem info
 *      OUT sector:   starting sector number of the file
 *      OUT size:     the size of the file in bytes
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int fat_file_open(int volid, const char *filename,
                  struct libfat_filesystem **fsinfo, libfat_sector_t *sector,
                  size_t *size)
{
   char shortname[FAT_SHORT_NAME_LEN];
   struct libfat_filesystem *fs;
   struct libfat_direntry dentry;
   struct fat_dirent *entry;
   libfat_sector_t sector_offset;
   int cluster;
   int status;

   status = get_boot_disk(&disk_info);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = get_volume_info(&disk_info, volid, &part_info);
   if (status != ERR_SUCCESS) {
      return status;
   }

   memset(&dfd, 0, sizeof (dfd));
   dfd.disk = &disk_info;
   dfd.partition = &part_info;

   fs = libfat_open(partition_read_handler, (intptr_t)&dfd,
                    disk_info.bytes_per_sector);
   if (fs == NULL) {
      return ERR_NOT_FOUND;
   }

   fat_get_shortname(filename, shortname);

   cluster = libfat_searchdir(fs, 0, shortname, &dentry);
   if (cluster == -1) {
      libfat_close(fs);
      return ERR_DEVICE_ERROR;
   } else if (cluster == -2) {
      libfat_close(fs);
      return ERR_NOT_FOUND;
   }

   sector_offset = libfat_clustertosector(fs, cluster);
   if (sector_offset == (libfat_sector_t)(-1)) {
      libfat_close(fs);
      return ERR_VOLUME_CORRUPTED;
   }

   entry = (struct fat_dirent *)&dentry.entry;
   *fsinfo = fs;
   *sector = sector_offset;
   *size = read32(&entry->size);

   return ERR_SUCCESS;
}

/*-- fat_file_load -------------------------------------------------------------
 *
 *      Load a file from a FAT filesystem.
 *
 * Parameters
 *      IN  volid:    MBR/GPT partition number of the volume to load from
 *      IN  filename: absolute path to the file
 *      IN  callback: routine to be called periodically while the file is being
 *                    loaded
 *      OUT buffer:   pointer to the buffer where the file was loaded
 *      OUT bufsize:  the size of the loaded buffer
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int fat_file_load(int volid, const char *filename,
                         int (*callback)(size_t), void **buffer,
                         size_t *bufsize)
{
   struct libfat_filesystem *fs;
   size_t count, len, n, size;
   libfat_sector_t sector;
   int status;
   void *data;
   char *bufp;
   disk_t disk;

   status = get_boot_disk(&disk);
   if (status != ERR_SUCCESS) {
      return status;
   }

   status = fat_file_open(volid, filename, &fs, &sector, &size);
   if (status != ERR_SUCCESS) {
      return status;
   }

   count = ceil(size, disk.bytes_per_sector);

   data = sys_malloc(count * disk.bytes_per_sector);
   if (data == NULL) {
      libfat_close(fs);
      return ERR_OUT_OF_RESOURCES;
   }

   bufp = data;

   for ( ; count > 0; count -= n) {
      n = MIN(count, READ_CHUNK_SIZE / disk.bytes_per_sector);
      status = fat_fread_sectors(fs, bufp, &sector, n);
      if (status != ERR_SUCCESS) {
         break;
      }

      len = n * disk.bytes_per_sector;
      bufp += len;

      if (callback != NULL && len > 0) {
         status = callback(len);
         if (status != ERR_SUCCESS) {
            break;
         }
      }
   }

   libfat_close(fs);

   if (status != ERR_SUCCESS) {
      sys_free(data);
   } else {
      *buffer = data;
      *bufsize = size;
   }

   return status;
}

/*-- fat_file_get_size ---------------------------------------------------------
 *
 *      Get the size of a file in a FAT filesystem.
 *
 * Parameters
 *      IN  volid:    MBR/GPT partition number of the volume to load from
 *      IN  filepath: absolute path to the file
 *      OUT filesize: the size of the file in bytes
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
static int fat_file_get_size(int volid, const char *filename,
                             size_t *filesize)
{
   struct libfat_filesystem *fs;
   libfat_sector_t sector;
   size_t size;
   int status;

   status = fat_file_open(volid, filename, &fs, &sector, &size);
   if (status != ERR_SUCCESS) {
      return status;
   }

   libfat_close(fs);

   *filesize = size;

   return status;
}

/*-- file_get_size_hint --------------------------------------------------------
 *
 *      Try to get the size of a file.
 *
 * Parameters
 *      IN  volid:    MBR/GPT partition number of the volume to load from
 *                    Setting this parameter to zero indicates that we want to
 *                    load a file from the boot volume. A parameter other than
 *                    zero indicates that the file should be loaded from a FAT
 *                    filesystem on the given partition
 *      IN  filepath: absolute path to the file
 *      OUT filesize: the size of the loaded buffer
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int file_get_size_hint(int volid, const char *filename, size_t *filesize)
{
   size_t size;
   int status;

   size = 0;

   if (volid != FIRMWARE_BOOT_VOLUME) {
      status = fat_file_get_size(volid, filename, &size);
   } else {
      status = firmware_file_get_size_hint(filename, &size);
   }

   if (status != ERR_SUCCESS) {
      return status;
   }

   *filesize = size;

   return ERR_SUCCESS;
}

/*-- file_load -----------------------------------------------------------------
 *
 *      Load a file into a freshly allocated memory buffer.
 *
 * Parameters
 *      IN  volid:            MBR/GPT partition number of the volume to load
 *                            from setting this parameter to zero indicates
 *                            that we want to load a file from the boot volume.
 *                            A parameter other than zero indicates that the
 *                            file should be loaded from a FAT filesystem on
 *                            the given partition
 *      IN  filepath:         absolute path to the file
 *      IN  callback:         routine to be called periodically while the file
 *                            is being loaded
 *      OUT buffer:           pointer to the buffer where the file was loaded
 *      OUT buffer:           pointer to the buffer where the file was loaded
 *      OUT bufsize:          the size of the loaded buffer
 *      OUT md5_compressed:   MD5 sum of compressed file. This parameter can be
 *                            NULL if the md5 sum is not to be calculated.
 *      OUT md5_uncompressed: MD5 sum of uncompressed file. This parameter can
 *                            be NULL if the md5 sum is not to be calculated.
 *
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int file_load(int volid, const char *filename, int (*callback)(size_t),
              void **buffer, size_t *bufsize, md5_t *md5_compressed,
              md5_t *md5_uncompressed)
{
   void *data, *dest;
   size_t size;
   int status;

   data = NULL;
   size = 0;

   if (volid != FIRMWARE_BOOT_VOLUME) {
      status = fat_file_load(volid, filename, callback, &data, &size);
   } else {
      status = firmware_file_read(filename, callback, &data, &size);
   }

   if (status != ERR_SUCCESS) {
      return status;
   }

   if (md5_compressed != NULL) {
      md5_compute(data, size, md5_compressed);
   }

   if (is_gzip(data, size)) {
      status = gzip_extract(data, size, &dest, &size);
      sys_free(data);

      if (status != ERR_SUCCESS) {
         return status;
      }

      data = dest;

      if (md5_uncompressed != NULL) {
         md5_compute(dest, size, md5_uncompressed);
      }
   }

   *bufsize = size;
   *buffer = data;

   return ERR_SUCCESS;
}

/*-- file_overwrite ------------------------------------------------------------
 *
 *      Overwrite a file which already exists with the new data contained in the
 *      given buffer.
 *
 * Parameters
 *      IN volid:    MBR/GPT partition number of the volume the file is on
 *      IN filepath: absolute path to the file
 *      IN buffer:   data buffer to read from
 *      IN buflen:   size of the data buffer, in bytes
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *----------------------------------------------------------------------------*/
int file_overwrite(int volid, const char *filepath, void *buffer, size_t buflen)
{
   char *sectorbuf;
   struct libfat_filesystem *fs;
   libfat_sector_t sector;
   size_t size;
   int status;
   disk_t disk;

   status = get_boot_disk(&disk);
   if (status != ERR_SUCCESS) {
      Log(LOG_DEBUG, "file_overwrite: get_boot_disk returned %d\n", status);
      return status;
   }

   if (buflen > disk.bytes_per_sector || volid == FIRMWARE_BOOT_VOLUME) {
      Log(LOG_DEBUG, "file_overwrite: buflen=%zd volid=%d\n", buflen, volid);
      return ERR_UNSUPPORTED;
   }

   status = fat_file_open(volid, filepath, &fs, &sector, &size);
   if (status != ERR_SUCCESS) {
      Log(LOG_DEBUG, "file_overwrite: fat_file_open returned %d\n", status);
      return status;
   }

   sectorbuf = sys_malloc(disk.bytes_per_sector);
   if (sectorbuf == NULL) {
      Log(LOG_DEBUG, "file_overwrite: sys_malloc failed\n");
      libfat_close(fs);
      return ERR_OUT_OF_RESOURCES;
   }

   sector += dfd.partition->info.start_lba;
   status = disk_read(dfd.disk, sectorbuf, sector, 1);
   if (status != ERR_SUCCESS) {
      Log(LOG_DEBUG, "file_overwrite: disk_read returned %d\n", status);

   } else /* disk_read succeeded */ {
      memcpy(sectorbuf, buffer, buflen);
      status = disk_write(dfd.disk, sectorbuf, sector, 1);
      if (status != ERR_SUCCESS) {
         Log(LOG_DEBUG, "file_overwrite: disk_write returned %d\n", status);
      }
   }

   libfat_close(fs);

   sys_free(sectorbuf);
   return status;
}

/*-- file_sanitize_path --------------------------------------------------------
 *
 *      Sanitize a UNIX-style or Protocol-style path
 *        - multiple-separator '/' occurences are merged
 *        - considers protocol paths with separator "://"
 *
 * Parameters
 *      IN  filepath: pointer to the UNIX/Protocol path
 *
 * Results
 *      ERR_SUCCESS, or a generic error status.
 *
 * Side Effects
 *      Input pathname is modified in place
 *----------------------------------------------------------------------------*/
int file_sanitize_path(char *filepath)
{
   const char *p;
   bool slash;
   int i, j;

   if (filepath == NULL) {
      return ERR_INVALID_PARAMETER;
   }

   slash = false;
   p = strstr(filepath, "://");
   j = 0;

   for (i = 0; filepath[i] != '\0'; i++) {
      if (isspace(filepath[i])) {
         continue;
      } else if (filepath[i] == '/' || filepath[i] == '\\') {
         if (!slash || ((p != NULL) && (&filepath[i] < (p + 3)))) {
            filepath[j++] = '/';
            slash = true;
         }
      } else {
         filepath[j++] = filepath[i];
         slash = false;
      }
   }

   filepath[j] = '\0';

   return ERR_SUCCESS;
}
